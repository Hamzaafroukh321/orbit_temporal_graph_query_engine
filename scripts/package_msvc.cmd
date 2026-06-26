@echo off
setlocal

if not defined VSCMD_ARG_TGT_ARCH (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
)

if /I not "%~1"=="--no-build" (
  call scripts\build_msvc.cmd || exit /b 1
)

set PKG=build\manual\package\orbit
set OUT=build\manual\package

if exist "%PKG%" rmdir /s /q "%PKG%"
mkdir "%PKG%\bin" || exit /b 1
mkdir "%PKG%\include\orbit" || exit /b 1
mkdir "%PKG%\lib" || exit /b 1
mkdir "%PKG%\examples" || exit /b 1

copy /y build\manual\orbit.exe "%PKG%\bin\orbit.exe" > nul || exit /b 1
copy /y build\manual\orbit.lib "%PKG%\lib\orbit.lib" > nul || exit /b 1
copy /y include\orbit\*.hpp "%PKG%\include\orbit\" > nul || exit /b 1
copy /y examples\embedded_history.cpp "%PKG%\examples\embedded_history.cpp" > nul || exit /b 1
copy /y README.md "%PKG%\README.md" > nul || exit /b 1

cl /std:c++20 /EHsc /W4 /WX /I "%PKG%\include" "%PKG%\examples\embedded_history.cpp" "%PKG%\lib\orbit.lib" /Fe"%OUT%\consumer_smoke.exe" || exit /b 1
"%OUT%\consumer_smoke.exe" > "%OUT%\consumer_smoke.out" || exit /b 1
findstr /C:"consumer_rows=1" "%OUT%\consumer_smoke.out" > nul || exit /b 1
