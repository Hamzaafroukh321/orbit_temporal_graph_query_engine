@echo off
setlocal

where cmake > nul 2> nul || (
  echo cmake not found on PATH
  exit /b 20
)

cmake --preset debug || exit /b 1
cmake --build --preset debug || exit /b 1
ctest --preset debug || exit /b 1

cmake --preset release || exit /b 1
cmake --build --preset release || exit /b 1

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
