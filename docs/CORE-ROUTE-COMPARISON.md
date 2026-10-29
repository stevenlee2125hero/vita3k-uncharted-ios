# Core route selection: source checks

The chosen base is [Plus all-enhancements a86b5f93](https://github.com/nckstwrt/Vita3K-Plus/tree/a86b5f93db43e4c4640ad1527e2a4695aad0f35b).
This repository retains its full history and graphics implementation. The iOS
runtime comes from [myfin555-code 432342d2](https://github.com/myfin555-code/Vita3K-iOS/tree/432342d2ef4f86ce98fad2b4362678d84cd4e204).
Its PLUS_GRAPHICS_PORT.md is a historical comparison; code in this repository
supersedes that partial graphics port. Compare the actual files and commits,
not the old document's integrated-feature list.

The partial runtime port supplied the native frontend, JIT/Oaknut adaptation,
SDL3 and MoltenVK packaging. Its source explicitly lacked the complete Plus
surface/depth reinterpretation path; transplanting it over the chosen Plus
renderer would discard the reason for choosing Plus. Integration therefore
retains full Plus surface_cache.cpp, GXM program snapshots, shader translator
and related types, with narrow staging-readback and platform adaptations.

Other routes inspected as source references on 2026-10-29:

* [Vion shader_graphics_subw 941791b1](https://github.com/vion-app-org/Vion/blob/941791b1d4c31caa190f7161520e216bfb24a9c0/vita3k/shader/src/translator/alu.cpp):
  vcomp uses the decoded_inst/repeat layout, materially different from the
  direct SPIR-V-emitting Plus visitor. Its code cannot be overlaid as a
  drop-in ALU patch. No game capture demonstrates an advantage over the full
  Plus translator for PCSD00001.
* [Vion vitagl-support 7606ed4e surface cache](https://github.com/vion-app-org/Vion/blob/7606ed4e85402aaa62d1aa0dadccbc5eb3de5007/vita3k/renderer/src/vulkan/surface_cache.cpp):
  the examined Vulkan implementation does not contain the Plus
  surface_cast_reinterpret, depth_alias_pack or preserve_f16_nan_as_u16
  paths. This is a targeted comparison of that source, not a claim that
  every Vion branch was exhaustively audited.
* [Vion native-ios-port e2650f91 CMake](https://github.com/vion-app-org/Vion/blob/e2650f91d9e942a17a59ef982861a475481d7ad5/vita3k/gui-ios/CMakeLists.txt):
  separate Swift frontend/ios_bridge, automatic team signing and embedded
  MoltenVK plus six FFmpeg XCFrameworks. Adopting it would introduce another
  host/build integration; no inspected code establishes that this improves
  character rendering or supplies a better iOS 26 StikDebug path.
* [Vita-emu 340f29f1 JIT policy](https://github.com/Vita-emu/Vita3K-iOS-/blob/340f29f1bf2d8a591676f294fd0d3436ede2a99b/vita3k/cpu/include/cpu/ios_jit_policy.h):
  memory-sensitive JIT sizing is relevant to smaller-memory phones. The
  selected runtime already exposes bounded JIT/thread/guest-memory budgets.
  This separate policy alone is insufficient reason to replace the core.

These checks justify the initial route; they do not prove that any fork runs
Golden Abyss correctly on an iPhone. No whole Vion/Vita-emu branch is merged.
No bootstrap-only emulator is used as a substitute for the commercial-game
core. The final acceptance evidence must come from the phone and game.
