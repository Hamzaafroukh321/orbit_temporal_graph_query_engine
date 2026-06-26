@echo off
setlocal

if /I not "%~1"=="--no-build" (
  call scripts\build_msvc.cmd || exit /b 1
)

if not exist build\manual\soak mkdir build\manual\soak
build\manual\orbit_soak.exe --cycles 25 --output build\manual\soak\soak.txt || exit /b 1
type build\manual\soak\soak.txt
