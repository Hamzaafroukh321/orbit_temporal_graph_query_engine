@echo off
setlocal

if /I not "%~1"=="--no-build" (
  call scripts\build_msvc.cmd || exit /b 1
)

if not exist build\manual\bench mkdir build\manual\bench
build\manual\orbit_benchmark.exe --check-smoke-budgets --output build\manual\bench\benchmark.txt || exit /b 1
type build\manual\bench\benchmark.txt
