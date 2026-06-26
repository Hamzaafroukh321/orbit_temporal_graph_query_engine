@echo off
setlocal

if not defined VSCMD_ARG_TGT_ARCH (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
)

if not exist build\manual mkdir build\manual

set COMMON=/std:c++20 /EHsc /W4 /WX /I include
set LIBSRC=src\base\checked.cpp src\base\error.cpp src\base\value.cpp src\format\ogr.cpp src\store\graph_store.cpp src\query\query.cpp

cl %COMMON% /I tests\unit %LIBSRC% tests\unit\test_main.cpp tests\unit\base_tests.cpp tests\unit\format_tests.cpp tests\integration\store_query_tests.cpp /Febuild\manual\orbit_unit_tests.exe || exit /b 1
cl %COMMON% %LIBSRC% src\cli\main.cpp /Febuild\manual\orbit.exe || exit /b 1
cl %COMMON% %LIBSRC% fuzz\fuzz_ogr_parser.cpp /Febuild\manual\orbit_ogr_parser_fuzz.exe || exit /b 1
cl %COMMON% %LIBSRC% fuzz\fuzz_graph_sequence.cpp /Febuild\manual\orbit_graph_sequence_fuzz.exe || exit /b 1
cl %COMMON% %LIBSRC% fuzz\fuzz_query_pipeline.cpp /Febuild\manual\orbit_query_pipeline_fuzz.exe || exit /b 1
cl %COMMON% %LIBSRC% tools\orbit_benchmark.cpp /Febuild\manual\orbit_benchmark.exe || exit /b 1
cl %COMMON% %LIBSRC% tools\orbit_soak.cpp /Febuild\manual\orbit_soak.exe || exit /b 1
lib /OUT:build\manual\orbit.lib checked.obj error.obj value.obj ogr.obj graph_store.obj query.obj || exit /b 1
