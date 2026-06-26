@echo off
setlocal

if /I not "%~1"=="--no-build" (
  call scripts\build_msvc.cmd || exit /b 1
)

set FIXTURE=fixtures\compat\v0_1
set OUT=build\manual\compat\v0_1
set GRAPH=%OUT%\graph.ogr

if not exist "%OUT%" mkdir "%OUT%"
if exist "%GRAPH%" del /f /q "%GRAPH%"

build\manual\orbit.exe init "%GRAPH%" > "%OUT%\init.out" || exit /b 1
build\manual\orbit.exe apply "%GRAPH%" "%FIXTURE%\changes.oms" > "%OUT%\apply.out" || exit /b 1

call :run_query service_scan 10 || exit /b 1
call :run_query one_hop_t10 10 || exit /b 1
call :run_query one_hop_t25 25 || exit /b 1
call :run_query cache_t25 25 || exit /b 1
call :run_explain path_explain 25 || exit /b 1
exit /b 0

:run_query
set NAME=%~1
set TIME=%~2
set /p QUERY=<"%FIXTURE%\queries\%NAME%.oqs"
build\manual\orbit.exe query "%GRAPH%" "%QUERY%" --time %TIME% > "%OUT%\%NAME%.out" || exit /b 1
fc "%OUT%\%NAME%.out" "%FIXTURE%\expected\%NAME%.out" > nul || exit /b 1
exit /b 0

:run_explain
set NAME=%~1
set TIME=%~2
set /p QUERY=<"%FIXTURE%\queries\%NAME%.oqs"
build\manual\orbit.exe explain "%GRAPH%" "%QUERY%" --time %TIME% > "%OUT%\%NAME%.out" || exit /b 1
fc "%OUT%\%NAME%.out" "%FIXTURE%\expected\%NAME%.out" > nul || exit /b 1
exit /b 0
