@echo off
setlocal

if /I not "%~1"=="--no-build" (
  call scripts\build_msvc.cmd || exit /b 1
)

set SMOKE_DIR=build\manual\cli_smoke
set GRAPH=%SMOKE_DIR%\graph.ogr
set CHANGES=%SMOKE_DIR%\changes.oms
set QUERY_OUT=%SMOKE_DIR%\query.out
set EXPLAIN_OUT=%SMOKE_DIR%\explain.out
set INSPECT_OUT=%SMOKE_DIR%\inspect.out

if not exist "%SMOKE_DIR%" mkdir "%SMOKE_DIR%"
if exist "%GRAPH%" del /f /q "%GRAPH%"

> "%CHANGES%" echo node 1 Service 0 100 tier=api
>> "%CHANGES%" echo node 2 Database 0 100 tier=db
>> "%CHANGES%" echo edge 10 1 2 DEPENDS 0 100 weight=3

build\manual\orbit.exe init "%GRAPH%" || exit /b 1
build\manual\orbit.exe apply "%GRAPH%" "%CHANGES%" || exit /b 1
build\manual\orbit.exe query "%GRAPH%" "FROM Service STEP OUT DEPENDS YIELD node.id" --time 10 > "%QUERY_OUT%" || exit /b 1
findstr /R /C:"^2$" "%QUERY_OUT%" > nul || exit /b 1
build\manual\orbit.exe explain "%GRAPH%" "FROM Service PATH OUT DEPENDS HOPS 2 COST weight YIELD path" --time 10 > "%EXPLAIN_OUT%" || exit /b 1
findstr /C:"cost-order" "%EXPLAIN_OUT%" > nul || exit /b 1
build\manual\orbit.exe check "%GRAPH%" || exit /b 1
build\manual\orbit.exe inspect "%GRAPH%" > "%INSPECT_OUT%" || exit /b 1
findstr /C:"latest_commit=1" "%INSPECT_OUT%" > nul || exit /b 1
