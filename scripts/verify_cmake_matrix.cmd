@echo off
setlocal

set CMAKE_BIN=C:\Program Files\CMake\bin
set NINJA_BIN=%LOCALAPPDATA%\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe
if exist "%CMAKE_BIN%\cmake.exe" set PATH=%CMAKE_BIN%;%PATH%
if exist "%NINJA_BIN%\ninja.exe" set PATH=%NINJA_BIN%;%PATH%

if not defined VSCMD_ARG_TGT_ARCH (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
)

where cmake > nul 2> nul || (
  echo cmake not found on PATH
  exit /b 20
)
where ninja > nul 2> nul || (
  echo ninja not found on PATH
  exit /b 21
)

cmake --preset debug || exit /b 1
cmake --build --preset debug || exit /b 1
ctest --preset debug || exit /b 1

cmake --preset release || exit /b 1
cmake --build --preset release || exit /b 1
cmake --install build/release --prefix build/install/release || exit /b 1
if not exist build\install\release\lib\cmake\orbit\orbitConfig.cmake exit /b 1

cmake --preset relwithdebinfo || exit /b 1
cmake --build --preset relwithdebinfo || exit /b 1
ctest --preset relwithdebinfo || exit /b 1

cmake --preset asan || exit /b 1
cmake --build --preset asan || exit /b 1
ctest --preset asan || exit /b 1

cmake --preset tsan || exit /b 1
cmake --build --preset tsan || exit /b 1
ctest --preset tsan || exit /b 1

cmake --preset coverage || exit /b 1
cmake --build --preset coverage || exit /b 1
ctest --preset coverage || exit /b 1

cmake --preset fuzz || exit /b 1
cmake --build --preset fuzz || exit /b 1
ctest --preset fuzz || exit /b 1
