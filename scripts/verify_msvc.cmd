@echo off
setlocal

call scripts\build_msvc.cmd || exit /b 1
call scripts\run_regression_matrix.cmd --no-build || exit /b 1
call scripts\benchmark_msvc.cmd --no-build || exit /b 1
call scripts\soak_msvc.cmd --no-build || exit /b 1
call scripts\compat_msvc.cmd --no-build || exit /b 1
call scripts\package_msvc.cmd --no-build || exit /b 1
