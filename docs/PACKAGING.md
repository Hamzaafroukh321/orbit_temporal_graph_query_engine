# Packaging

## CMake

The CMake install exports the public library target and CLI:

```sh
cmake --preset release
cmake --build --preset release
cmake --install build/release --prefix install/orbit
```

Installed consumers can use:

```cmake
find_package(orbit CONFIG REQUIRED)
target_link_libraries(app PRIVATE orbit::orbit)
```

This path is scripted by `scripts\verify_cmake_matrix.cmd`, but it remains
blocked in the current agent environment because CMake is not on `PATH`.

## Manual MSVC Smoke

When Visual Studio Build Tools are available, run:

```bat
scripts\package_msvc.cmd
```

The script creates `build\manual\package\orbit`, copies headers, `orbit.exe`,
`orbit.lib`, and `examples\embedded_history.cpp`, then compiles and runs the
example as an external public-API consumer.
