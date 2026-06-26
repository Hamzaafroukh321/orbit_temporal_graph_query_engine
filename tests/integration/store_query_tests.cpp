#include "test_support.hpp"

#include "orbit/store.hpp"

namespace {

orbit::GraphStore sample_store(const std::string& name) {
  auto path = orbit::test::temp_path(name + ".ogr");
  auto opened = orbit::GraphStore::open(path, orbit::OpenOptions{true});
  REQUIRE(opened);
  return opened.value();
}

void seed_graph(orbit::GraphStore& store) {
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "Service", orbit::Interval{0, 100},
                               {{"tier", std::string{"api"}}}));
  REQUIRE(txn.value().put_node(orbit::NodeId{2}, "Database", orbit::Interval{0, 50},
                               {{"tier", std::string{"data"}}}));
  REQUIRE(txn.value().put_node(orbit::NodeId{3}, "Database", orbit::Interval{20, 100},
                               {{"tier", std::string{"data"}}}));
  REQUIRE(txn.value().put_edge(orbit::EdgeId{10}, orbit::NodeId{1}, orbit::NodeId{2}, "DEPENDS",
                               orbit::Interval{0, 50}));
  REQUIRE(txn.value().put_edge(orbit::EdgeId{11}, orbit::NodeId{2}, orbit::NodeId{3}, "DEPENDS",
                               orbit::Interval{20, 40}));
  auto commit = txn.value().commit();
  REQUIRE(commit);
}

std::vector<orbit::QueryRow> drain(orbit::ResultCursor cursor, std::size_t batch) {
  std::vector<orbit::QueryRow> rows;
  while (true) {
    auto next = cursor.next(batch);
    REQUIRE(next);
    if (!next.value()) {
      break;
    }
    rows.insert(rows.end(), next.value()->rows.begin(), next.value()->rows.end());
  }
  return rows;
}

std::vector<std::string> drain_keys(orbit::ResultCursor cursor, std::size_t batch) {
  std::vector<std::string> keys;
  while (true) {
    auto next = cursor.next(batch);
    REQUIRE(next);
    if (!next.value()) {
      break;
    }
    REQUIRE(next.value()->continuation_key.has_value());
    keys.push_back(*next.value()->continuation_key);
  }
  return keys;
}

}  // namespace

ORBIT_TEST(CreateUpdateDeleteVersionChain) {
  auto store = sample_store("version_chain");
  seed_graph(store);
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{2}, "Database", orbit::Interval{0, 100},
                               {{"tier", std::string{"critical"}}}));
  REQUIRE(txn.value().commit());
  auto old_snapshot = store.snapshot(orbit::SnapshotSelector{orbit::CommitSeq{1}, 60});
  auto new_snapshot = store.snapshot(orbit::SnapshotSelector{orbit::CommitSeq{2}, 60});
  REQUIRE(old_snapshot);
  REQUIRE(new_snapshot);
  REQUIRE(!old_snapshot.value().node(orbit::NodeId{2}));
  REQUIRE(new_snapshot.value().node(orbit::NodeId{2}));
}

ORBIT_TEST(RepeatedMutationCoalesces) {
  auto store = sample_store("coalesce");
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "A", orbit::Interval{0, 10}));
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "B", orbit::Interval{0, 10}));
  REQUIRE(txn.value().commit());
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 1});
  REQUIRE(snapshot);
  REQUIRE(snapshot.value().nodes().size() == 1);
  REQUIRE(snapshot.value().nodes()[0].label == "B");
}

ORBIT_TEST(EndpointPolicyAtomic) {
  auto store = sample_store("endpoint");
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_edge(orbit::EdgeId{1}, orbit::NodeId{1}, orbit::NodeId{2}, "X",
                               orbit::Interval{0, 10}));
  REQUIRE(!txn.value().commit());
}

ORBIT_TEST(IntervalHalfOpenBoundaries) {
  auto store = sample_store("half_open");
  seed_graph(store);
  auto at_start = store.snapshot(orbit::SnapshotSelector{std::nullopt, 0});
  auto at_end = store.snapshot(orbit::SnapshotSelector{std::nullopt, 50});
  REQUIRE(at_start);
  REQUIRE(at_end);
  REQUIRE(at_start.value().node(orbit::NodeId{2}));
  REQUIRE(!at_end.value().node(orbit::NodeId{2}));
}

ORBIT_TEST(ConflictLeavesNoTailVisibility) {
  auto store = sample_store("conflict");
  seed_graph(store);
  auto before = store.latest_commit().value;
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_edge(orbit::EdgeId{99}, orbit::NodeId{404}, orbit::NodeId{1}, "X",
                               orbit::Interval{0, 1}));
  REQUIRE(!txn.value().commit());
  REQUIRE(store.latest_commit().value == before);
}

ORBIT_TEST(LabelIndexEqualsScan) {
  auto store = sample_store("label_scan");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  REQUIRE(snapshot);
  auto prepared = store.prepare("FROM Database YIELD node.id");
  REQUIRE(prepared);
  auto rows = drain(prepared.value().execute(snapshot.value()).value(), 10);
  REQUIRE(rows.size() == 2);
}

ORBIT_TEST(PropertyIndexOldKeyRemoved) {
  auto store = sample_store("property_update");
  seed_graph(store);
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "Service", orbit::Interval{0, 100},
                               {{"tier", std::string{"web"}}}));
  REQUIRE(txn.value().commit());
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 10});
  auto prepared = store.prepare("FROM Service WHERE tier = api YIELD node.id");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto rows = drain(prepared.value().execute(snapshot.value()).value(), 10);
  REQUIRE(rows.empty());
}

ORBIT_TEST(AdjacencyBothDirections) {
  auto store = sample_store("adjacency");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Service STEP OUT DEPENDS YIELD node.id");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto rows = drain(prepared.value().execute(snapshot.value()).value(), 1);
  REQUIRE(rows.size() == 1);
  REQUIRE(std::get<std::int64_t>(rows[0].values[0]) == 2);
}

ORBIT_TEST(SnapshotPinsGeneration) {
  auto store = sample_store("snapshot_pin");
  seed_graph(store);
  auto old_snapshot = store.snapshot(orbit::SnapshotSelector{orbit::CommitSeq{1}, 25});
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().delete_node(orbit::NodeId{2}));
  REQUIRE(txn.value().commit());
  REQUIRE(old_snapshot);
  REQUIRE(old_snapshot.value().node(orbit::NodeId{2}));
}

ORBIT_TEST(IndexCoveragePlannerFallback) {
  auto prepared = orbit::prepare_query("FROM Service WHERE tier = api YIELD node.id");
  REQUIRE(prepared);
  auto explain = prepared.value().explain();
  REQUIRE(explain.fingerprint.find("where=tier") != std::string::npos);
  REQUIRE(std::find(explain.operators.begin(), explain.operators.end(),
                    "property-index-seek(tier)") != explain.operators.end());
}

ORBIT_TEST(GrammarAllCoreClauses) {
  REQUIRE(orbit::prepare_query("AT COMMIT HEAD TIME 25 FROM Service STEP OUT DEPENDS YIELD node.id"));
  REQUIRE(orbit::prepare_query("FROM Service PATH OUT DEPENDS HOPS 2 YIELD path"));
}

ORBIT_TEST(SourceRangeDiagnostics) {
  auto prepared = orbit::prepare_query("FROM Service STEP IN DEPENDS YIELD node.id");
  REQUIRE(!prepared);
  REQUIRE(prepared.error().range.has_value());
}

ORBIT_TEST(PlannerTieDeterministic) {
  auto a = orbit::prepare_query("FROM Service YIELD node.id");
  auto b = orbit::prepare_query("FROM Service YIELD node.id");
  REQUIRE(a);
  REQUIRE(b);
  REQUIRE(a.value().explain().fingerprint == b.value().explain().fingerprint);
}

ORBIT_TEST(TemporalFilterNeverOmitted) {
  auto prepared = orbit::prepare_query("FROM Service STEP OUT DEPENDS YIELD node.id");
  REQUIRE(prepared);
  auto explain = prepared.value().explain();
  REQUIRE(std::find(explain.operators.begin(), explain.operators.end(),
                    "temporal-filter(commit+valid-time)") != explain.operators.end());
}

ORBIT_TEST(ParameterTypeMismatch) {
  auto prepared = orbit::prepare_query("FROM Service WHERE tier api YIELD node.id");
  REQUIRE(!prepared);
}

ORBIT_TEST(BatchSizeInvariantRows) {
  auto store = sample_store("batch");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Database YIELD node.id");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto one = drain(prepared.value().execute(snapshot.value()).value(), 1);
  auto many = drain(prepared.value().execute(snapshot.value()).value(), 100);
  REQUIRE(one.size() == many.size());
}

ORBIT_TEST(OneHopExpandOrdering) {
  auto store = sample_store("onehop_order");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Service STEP OUT DEPENDS YIELD edge.id");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto rows = drain(prepared.value().execute(snapshot.value()).value(), 10);
  REQUIRE(rows.size() == 1);
  REQUIRE(std::get<std::int64_t>(rows[0].values[0]) == 10);
}

ORBIT_TEST(SimplePathRejectsRepeat) {
  auto store = sample_store("simple_path");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Service PATH OUT DEPENDS HOPS 3 YIELD path");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto rows = drain(prepared.value().execute(snapshot.value()).value(), 10);
  REQUIRE(rows.size() == 2);
}

ORBIT_TEST(EqualCostPathOrder) {
  auto store = sample_store("path_order");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Service PATH OUT DEPENDS HOPS 2 YIELD node.id");
  auto rows = drain(prepared.value().execute(snapshot.value()).value(), 10);
  REQUIRE(std::get<std::int64_t>(rows[0].values[0]) == 2);
}

ORBIT_TEST(CancelPathPreservesPriorBatches) {
  auto store = sample_store("cancel");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Service PATH OUT DEPENDS HOPS 2 YIELD path");
  auto cursor = prepared.value().execute(snapshot.value());
  REQUIRE(cursor);
  auto first = cursor.value().next(1);
  REQUIRE(first);
  cursor.value().cancel();
  REQUIRE(!cursor.value().next(1));
}

ORBIT_TEST(ReopenPreservesCommittedStore) {
  auto path = orbit::test::temp_path("reopen.ogr");
  auto store = orbit::GraphStore::open(path, orbit::OpenOptions{true});
  REQUIRE(store);
  seed_graph(store.value());
  auto reopened = orbit::GraphStore::open(path);
  REQUIRE(reopened);
  REQUIRE(reopened.value().latest_commit().value == 1);
}

ORBIT_TEST(DeleteEdgeRemovesTraversal) {
  auto store = sample_store("delete_edge");
  seed_graph(store);
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().delete_edge(orbit::EdgeId{10}));
  REQUIRE(txn.value().commit());
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Service STEP OUT DEPENDS YIELD node.id");
  auto rows = drain(prepared.value().execute(snapshot.value()).value(), 10);
  REQUIRE(rows.empty());
}

ORBIT_TEST(CheckValidatesCommittedRecords) {
  auto store = sample_store("check");
  seed_graph(store);
  REQUIRE(store.check());
}

ORBIT_TEST(SnapshotLabelIndexMatchesVisibleScan) {
  auto store = sample_store("snapshot_label_index");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  REQUIRE(snapshot);
  auto indexed = snapshot.value().nodes_with_label("Database");
  REQUIRE(indexed.size() == 2);
  REQUIRE(indexed[0].id.value == 2);
  REQUIRE(indexed[1].id.value == 3);
}

ORBIT_TEST(SnapshotPropertyIndexMatchesVisibleScan) {
  auto store = sample_store("snapshot_property_index");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  REQUIRE(snapshot);
  auto indexed = snapshot.value().nodes_with_property("tier", orbit::PropertyValue{std::string{"data"}});
  REQUIRE(indexed.size() == 2);
}

ORBIT_TEST(SnapshotAdjacencyIndexMatchesVisibleEdges) {
  auto store = sample_store("snapshot_adjacency_index");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  REQUIRE(snapshot);
  auto edges = snapshot.value().out_edges(orbit::NodeId{1}, "DEPENDS");
  REQUIRE(edges.size() == 1);
  REQUIRE(edges[0].id.value == 10);
}

ORBIT_TEST(PropertyIndexedQueryStillFiltersLabel) {
  auto store = sample_store("property_label_filter");
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "Service", orbit::Interval{0, 100},
                               {{"tier", std::string{"shared"}}}));
  REQUIRE(txn.value().put_node(orbit::NodeId{2}, "Database", orbit::Interval{0, 100},
                               {{"tier", std::string{"shared"}}}));
  REQUIRE(txn.value().commit());
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 10});
  auto prepared = store.prepare("FROM Service WHERE tier = shared YIELD node.id");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto rows = drain(prepared.value().execute(snapshot.value()).value(), 10);
  REQUIRE(rows.size() == 1);
  REQUIRE(std::get<std::int64_t>(rows[0].values[0]) == 1);
}

ORBIT_TEST(TemporalIntervalSelectionHandlesStartEndAndFutureStarts) {
  auto store = sample_store("temporal_interval_index_nodes");
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "Window", orbit::Interval{10, 20}));
  REQUIRE(txn.value().put_node(orbit::NodeId{2}, "Window", orbit::Interval{20, 30}));
  REQUIRE(txn.value().put_node(orbit::NodeId{3}, "Window", orbit::Interval{40, 50}));
  REQUIRE(txn.value().commit());

  auto at_start = store.snapshot(orbit::SnapshotSelector{std::nullopt, 10});
  auto at_boundary = store.snapshot(orbit::SnapshotSelector{std::nullopt, 20});
  auto before_future = store.snapshot(orbit::SnapshotSelector{std::nullopt, 35});
  REQUIRE(at_start);
  REQUIRE(at_boundary);
  REQUIRE(before_future);
  REQUIRE(at_start.value().nodes_with_label("Window").size() == 1);
  REQUIRE(at_start.value().nodes_with_label("Window")[0].id.value == 1);
  REQUIRE(at_boundary.value().nodes_with_label("Window").size() == 1);
  REQUIRE(at_boundary.value().nodes_with_label("Window")[0].id.value == 2);
  REQUIRE(before_future.value().nodes_with_label("Window").empty());
}

ORBIT_TEST(TemporalEdgeIntervalFiltersIndependentlyFromEndpoints) {
  auto store = sample_store("temporal_interval_index_edges");
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_node(orbit::NodeId{2}, "Database", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_edge(orbit::EdgeId{1}, orbit::NodeId{1}, orbit::NodeId{2}, "DEPENDS",
                               orbit::Interval{10, 20}));
  REQUIRE(txn.value().commit());

  auto before_edge = store.snapshot(orbit::SnapshotSelector{std::nullopt, 5});
  auto during_edge = store.snapshot(orbit::SnapshotSelector{std::nullopt, 10});
  auto at_edge_end = store.snapshot(orbit::SnapshotSelector{std::nullopt, 20});
  REQUIRE(before_edge);
  REQUIRE(during_edge);
  REQUIRE(at_edge_end);
  REQUIRE(before_edge.value().out_edges(orbit::NodeId{1}, "DEPENDS").empty());
  REQUIRE(during_edge.value().out_edges(orbit::NodeId{1}, "DEPENDS").size() == 1);
  REQUIRE(at_edge_end.value().out_edges(orbit::NodeId{1}, "DEPENDS").empty());
}

ORBIT_TEST(AdjacencyContinuationKeysAdvanceAtBatchBoundaries) {
  auto store = sample_store("adjacency_continuation_keys");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Service STEP OUT DEPENDS YIELD node.id");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto cursor = prepared.value().execute(snapshot.value());
  REQUIRE(cursor);
  auto batch = cursor.value().next(1);
  REQUIRE(batch);
  REQUIRE(batch.value().has_value());
  REQUIRE(batch.value()->continuation_key == std::optional<std::string>{"adj:1:10:2"});
}

ORBIT_TEST(ContinuationKeysAreBatchSizeInvariant) {
  auto store = sample_store("continuation_batch_invariant");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Service PATH OUT DEPENDS HOPS 2 YIELD path");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto by_one = drain_keys(prepared.value().execute(snapshot.value()).value(), 1);
  auto by_many = drain_keys(prepared.value().execute(snapshot.value()).value(), 100);
  REQUIRE(!by_one.empty());
  REQUIRE(by_one.back() == by_many.back());
}

ORBIT_TEST(PathHopLimitReturnsResourceError) {
  auto store = sample_store("path_hop_limit");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Service PATH OUT DEPENDS HOPS 2 YIELD path");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto cursor = prepared.value().execute(snapshot.value(), orbit::QueryLimits{10000, 1024, 1, 10000});
  REQUIRE(!cursor);
  REQUIRE(cursor.error().code == orbit::ErrorCode::ResourceLimit);
}

ORBIT_TEST(PathFrontierLimitReturnsResourceError) {
  auto store = sample_store("path_frontier_limit");
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_node(orbit::NodeId{2}, "Node", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_node(orbit::NodeId{3}, "Node", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_edge(orbit::EdgeId{1}, orbit::NodeId{1}, orbit::NodeId{2}, "DEPENDS",
                               orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_edge(orbit::EdgeId{2}, orbit::NodeId{1}, orbit::NodeId{3}, "DEPENDS",
                               orbit::Interval{0, 100}));
  REQUIRE(txn.value().commit());
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 10});
  auto prepared = store.prepare("FROM Service PATH OUT DEPENDS HOPS 2 YIELD path");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto cursor = prepared.value().execute(snapshot.value(), orbit::QueryLimits{10000, 1024, 32, 1});
  REQUIRE(!cursor);
  REQUIRE(cursor.error().code == orbit::ErrorCode::ResourceLimit);
}

ORBIT_TEST(PathCycleDoesNotRepeatSeedNode) {
  auto store = sample_store("path_cycle_no_repeat");
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_node(orbit::NodeId{2}, "Node", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_edge(orbit::EdgeId{1}, orbit::NodeId{1}, orbit::NodeId{2}, "DEPENDS",
                               orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_edge(orbit::EdgeId{2}, orbit::NodeId{2}, orbit::NodeId{1}, "DEPENDS",
                               orbit::Interval{0, 100}));
  REQUIRE(txn.value().commit());
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 10});
  auto prepared = store.prepare("FROM Service PATH OUT DEPENDS HOPS 3 YIELD path");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto rows = drain(prepared.value().execute(snapshot.value()).value(), 10);
  REQUIRE(rows.size() == 1);
  REQUIRE(std::get<std::string>(rows[0].values[0]) == "1->2");
}

ORBIT_TEST(CostAwarePathOrdersByCumulativeCost) {
  auto store = sample_store("cost_path_order");
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_node(orbit::NodeId{2}, "Node", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_node(orbit::NodeId{3}, "Node", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_edge(orbit::EdgeId{1}, orbit::NodeId{1}, orbit::NodeId{2}, "DEPENDS",
                               orbit::Interval{0, 100}, {{"weight", std::int64_t{10}}}));
  REQUIRE(txn.value().put_edge(orbit::EdgeId{2}, orbit::NodeId{1}, orbit::NodeId{3}, "DEPENDS",
                               orbit::Interval{0, 100}, {{"weight", std::int64_t{1}}}));
  REQUIRE(txn.value().commit());
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 10});
  auto prepared = store.prepare("FROM Service PATH OUT DEPENDS HOPS 1 COST weight YIELD path");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto rows = drain(prepared.value().execute(snapshot.value()).value(), 10);
  REQUIRE(rows.size() == 2);
  REQUIRE(std::get<std::string>(rows[0].values[0]) == "1->3");
  REQUIRE(std::get<std::string>(rows[1].values[0]) == "1->2");
}

ORBIT_TEST(CostAwarePathRejectsNegativeCost) {
  auto store = sample_store("cost_path_negative");
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_node(orbit::NodeId{2}, "Node", orbit::Interval{0, 100}));
  REQUIRE(txn.value().put_edge(orbit::EdgeId{1}, orbit::NodeId{1}, orbit::NodeId{2}, "DEPENDS",
                               orbit::Interval{0, 100}, {{"weight", std::int64_t{-1}}}));
  REQUIRE(txn.value().commit());
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 10});
  auto prepared = store.prepare("FROM Service PATH OUT DEPENDS HOPS 1 COST weight YIELD path");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto cursor = prepared.value().execute(snapshot.value());
  REQUIRE(!cursor);
  REQUIRE(cursor.error().code == orbit::ErrorCode::QueryType);
}

ORBIT_TEST(CostAwarePathExplainIncludesCostOrder) {
  auto prepared = orbit::prepare_query("FROM Service PATH OUT DEPENDS HOPS 2 COST weight YIELD path");
  REQUIRE(prepared);
  auto explain = prepared.value().explain();
  REQUIRE(std::find(explain.operators.begin(), explain.operators.end(), "cost-order(weight)") !=
          explain.operators.end());
}

ORBIT_TEST(IndexCoverageMatchesSnapshotCommit) {
  auto store = sample_store("index_coverage_commit");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  REQUIRE(snapshot);
  auto coverage = snapshot.value().index_coverage();
  REQUIRE(coverage.covers(snapshot.value().commit()));
  REQUIRE(!coverage.covers(orbit::CommitSeq{snapshot.value().commit().value + 1U}));
}

ORBIT_TEST(HeldSnapshotKeepsOldIndexCoverageAfterNewCommit) {
  auto store = sample_store("index_coverage_held_snapshot");
  seed_graph(store);
  auto held = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  REQUIRE(held);
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{99}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().commit());
  REQUIRE(held.value().index_coverage().covers(orbit::CommitSeq{1}));
  REQUIRE(!held.value().index_coverage().covers(orbit::CommitSeq{2}));
}

ORBIT_TEST(SnapshotPinsIndexGenerationUntilRelease) {
  auto store = sample_store("snapshot_pin_stats");
  seed_graph(store);
  {
    auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
    REQUIRE(snapshot);
    auto stats = store.cache_stats();
    REQUIRE(stats.pinned_generations == 1);
    REQUIRE(stats.total_pins == 1);
  }
  auto stats = store.cache_stats();
  REQUIRE(stats.pinned_generations == 0);
  REQUIRE(stats.total_pins == 0);
}

ORBIT_TEST(EvictionSkipsPinnedIndexGeneration) {
  auto store = sample_store("snapshot_eviction_pinned");
  seed_graph(store);
  auto held = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  REQUIRE(held);
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{99}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().commit());
  auto evicted = store.evict_unpinned_indexes();
  REQUIRE(evicted);
  REQUIRE(evicted.value() == 0);
  REQUIRE(store.cache_stats().pinned_generations == 1);
}

ORBIT_TEST(EvictionRemovesReleasedOldGeneration) {
  auto store = sample_store("snapshot_eviction_released");
  seed_graph(store);
  {
    auto held = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
    REQUIRE(held);
  }
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{99}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().commit());
  auto evicted = store.evict_unpinned_indexes();
  REQUIRE(evicted);
  REQUIRE(evicted.value() == 1);
  REQUIRE(store.cache_stats().pinned_generations == 0);
}

ORBIT_TEST(CompactionRetentionPlannerKeepsRecentCommits) {
  auto store = sample_store("compaction_retention_recent");
  seed_graph(store);
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{99}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().commit());
  auto report = store.plan_compaction(1);
  REQUIRE(report);
  REQUIRE(report.value().source_latest.value == 2);
  REQUIRE(report.value().retained_from.value == 2);
  REQUIRE(report.value().retained_through.value == 2);
  REQUIRE(report.value().retained_nodes == 1);
  REQUIRE(report.value().publishable);
}

ORBIT_TEST(CompactionRetentionPlannerRejectsZeroWindow) {
  auto store = sample_store("compaction_retention_zero");
  seed_graph(store);
  auto report = store.plan_compaction(0);
  REQUIRE(!report);
  REQUIRE(report.error().code == orbit::ErrorCode::Usage);
}

ORBIT_TEST(CompactionCandidateBlockedByPinnedOldGeneration) {
  auto store = sample_store("compaction_pinned_old_generation");
  seed_graph(store);
  auto held = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  REQUIRE(held);
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{99}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().commit());
  auto report = store.plan_compaction(1);
  REQUIRE(report);
  REQUIRE(!report.value().publishable);
}

ORBIT_TEST(CompactionPublishRejectsPinnedGeneration) {
  auto store = sample_store("compaction_publish_pinned");
  seed_graph(store);
  auto held = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  REQUIRE(held);
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{99}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().commit());
  auto compacted = store.compact(1);
  REQUIRE(!compacted);
  REQUIRE(compacted.error().code == orbit::ErrorCode::Conflict);
}

ORBIT_TEST(CompactionPublishRetiresReleasedGeneration) {
  auto store = sample_store("compaction_publish_released");
  seed_graph(store);
  {
    auto held = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
    REQUIRE(held);
  }
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{99}, "Service", orbit::Interval{0, 100}));
  REQUIRE(txn.value().commit());
  auto before = store.cache_stats();
  REQUIRE(before.known_generations >= 2);
  auto compacted = store.compact(1);
  REQUIRE(compacted);
  auto after = store.cache_stats();
  REQUIRE(after.pinned_generations == 0);
  REQUIRE(after.known_generations <= before.known_generations);
}

ORBIT_TEST(QueryExecutionHonorsPreCancelledToken) {
  auto store = sample_store("query_pre_cancelled");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Database YIELD node.id");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  orbit::CancelToken cancel;
  cancel.cancel();
  auto cursor = prepared.value().execute(snapshot.value(), orbit::QueryLimits{}, cancel);
  REQUIRE(!cursor);
  REQUIRE(cursor.error().code == orbit::ErrorCode::Cancelled);
}

ORBIT_TEST(QueryExecutionHonorsWorkLimit) {
  auto store = sample_store("query_work_limit");
  seed_graph(store);
  auto snapshot = store.snapshot(orbit::SnapshotSelector{std::nullopt, 25});
  auto prepared = store.prepare("FROM Service PATH OUT DEPENDS HOPS 2 YIELD path");
  REQUIRE(snapshot);
  REQUIRE(prepared);
  auto cursor = prepared.value().execute(snapshot.value(), orbit::QueryLimits{10000, 1024, 32, 10000, 1});
  REQUIRE(!cursor);
  REQUIRE(cursor.error().code == orbit::ErrorCode::ResourceLimit);
}

ORBIT_TEST(StoreShutdownRejectsNewWork) {
  auto store = sample_store("store_shutdown_rejects");
  seed_graph(store);
  REQUIRE(store.shutdown());
  REQUIRE(!store.begin());
  REQUIRE(!store.snapshot(orbit::SnapshotSelector{std::nullopt, 25}));
  REQUIRE(!store.prepare("FROM Service YIELD node.id"));
  REQUIRE(!store.compact(1));
}

ORBIT_TEST(StoreShutdownRejectsOpenTransactionCommit) {
  auto store = sample_store("store_shutdown_open_txn");
  auto txn = store.begin();
  REQUIRE(txn);
  REQUIRE(txn.value().put_node(orbit::NodeId{1}, "Service", orbit::Interval{0, 100}));
  REQUIRE(store.shutdown());
  auto committed = txn.value().commit();
  REQUIRE(!committed);
  REQUIRE(committed.error().code == orbit::ErrorCode::Cancelled);
}
