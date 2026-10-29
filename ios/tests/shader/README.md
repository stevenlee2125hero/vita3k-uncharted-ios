# Shader regressions for the iOS Plus port

This optional host suite compiles the **actual** GXM and shader libraries with
repository-pinned glslang, SPIRV-Cross, fmt, spdlog and Boost. It does not boot the
emulator or require game files. The only replacement is the optional shader CLI
file reader (`FileReader.cpp`, std::ifstream instead of SDL). Production logging headers
and Boost path sources are compiled; the full logging implementation is part of the application build.

Requirements: CMake 3.22+, C++20 compiler, Python 3, SPIRV-Tools (`spirv-val` and
`spirv-dis`). On Amazon Linux, `sudo dnf install spirv-tools` supplies the latter.

From the repository root:

```sh
git submodule update --init --depth 1 external/glslang external/SPIRV-Cross external/fmt external/spdlog external/boost
cmake -S ios/tests/shader -B build-shader-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build-shader-tests -j 4
ctest --test-dir build-shader-tests --output-on-failure
```

Tests cover synthetic GXP uniform layouts, USSE predicates/branches, base-2
complex arithmetic, conditional vector moves, integer instruction repetition,
initialized registers, signed GPU address addition and mapped byte/half/word
loads/stores. Generated Vulkan 1.0 SPIR-V is validated with SPIRV-Tools. The GXP
fixtures are also translated to iOS Metal source by pinned SPIRV-Cross.
`ValidateSpirv.py` evaluates the small straight-line address fixture against
independent native 64-bit sums; it is not a general shader or GPU emulator.

The suite also translates all ten bundled Plus SPIR-V shaders, including
surface reinterpretation and depth alias packing, to iOS Metal. CI sends both
builtin and synthetic Metal output to Apple's iPhoneOS Metal compiler.
This does **not** execute shaders on an iPhone, validate Vulkan synchronization
on a GPU, or establish game compatibility/FPS.
The native suite is `cmake -S ios/tests -B build-tests`. Initialize
external/LibAtrac9 and apply ios/patches/0002-atrac9-bounded-input.patch first,
as delivery CI does, for its bounded-input audio regression test.
