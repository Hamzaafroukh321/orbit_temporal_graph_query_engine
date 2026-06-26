@echo off
setlocal

if /I not "%~1"=="--no-build" (
  call scripts\build_msvc.cmd || exit /b 1
)

build\manual\orbit_unit_tests.exe || exit /b 1
build\manual\orbit_ogr_parser_fuzz.exe corpus || exit /b 1
build\manual\orbit_graph_sequence_fuzz.exe corpus || exit /b 1
build\manual\orbit_query_pipeline_fuzz.exe corpus || exit /b 1
call scripts\run_cli_smoke.cmd --no-build || exit /b 1
call scripts\run_fault_matrix.cmd --no-build || exit /b 1
