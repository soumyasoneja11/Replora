Configure, build, and test the C++17 engine from the repository root.

    cmake -S cpp -B build/native
    cmake --build build/native
    ctest --test-dir build/native --output-on-failure

Needs CMake 3.20 or newer, a C++17 compiler, and network on first configure.
GoogleTest is fetched at tag v1.15.2. Warnings are -Wall -Wextra, or /W4 on MSVC.
replora_core is the static library built from cpp/src. CLI and WASM are commented placeholders.
Key is std::int32_t, the integer form of the TypeScript number keys in src/simulation/types.ts.
