# PCSD00001: implementation and device acceptance

## What changed

The base is the complete Vita3K-Plus all-enhancements revision a86b5f93,
not a historical stevenlee iOS repair branch. See NEW-CORE-PROVENANCE.md for
the full source revisions and integration records. The entire Plus ALU,
GXP/USSE translator, surface reinterpretation, raw F16 and depth/stencil
implementation are retained. They address classes of material/lighting and
surface-alias errors, but have not been shown to fix this phone's magenta
characters. The old screenshot symptom alone does not identify the cause.

Additional code changes and their evidence:

| Area / primary files | Change and scope | Evidence / remaining gap |
| --- | --- | --- |
| renderer/src/texture/format.cpp | BC2/BC3 always use four-colour interpolation; BC1 alone uses its transparent endpoint mode. Clip partial 4x4 block copies to the actual destination width/height. | Synthetic palette anchors and 504 exact-size decompression cases under ASan/UBSan; no captured PCSD00001 texture proves this was its material bug. |
| renderer/src/vulkan/surface_cache.cpp; include/renderer/vulkan/unmapped_readback.h | iOS staging readback retains Plus repacking, tiles and partial-surface contents; add transfer-to-host visibility, invalidation and allocation generation checks. | Linear/tiled footprint and bounds tests. Device synchronization and scene transitions remain unverified. |
| renderer/src/vulkan/context.cpp | Submit and wait for unmapped surface-sync jobs, rather than gating those jobs on direct GPU memory mapping. | Code path audit and full application build; no GPU execution test on this Windows host. |
| kernel thread/state code; cpu/JIT code; ios/patches | Integrate iOS JIT/runtime while preserving Plus scheduling. Wait for registered host thread bodies to exit before teardown. Reuse bounded JIT slots. | Runtime-budget/frame-lifetime support tests; actual StikDebug attachment and repeated game launches require device testing. |
| renderer/src/vulkan/renderer.cpp | PCSD00001 selects full-precision shaders and native 1x resolution. | Title-ID gate in source; performance/visual result unverified. |
| ios/CMakeLists.txt; Info.plist.in; shader/spirv_recompiler.h | Independent Bundle ID, Documents/UnchartedPSV and shader namespace 1001. Ship all Plus builtin shaders and provenance. | IPA verifier checks identity, arm64 device binary, bundled shader bytes, CRC and SHA256. |
| ios/tests/shader; tools/test_texture_formats.py; ios/tests | Compile the production shader translator and test texture formats, cache data, guest bounds, frame ownership and readback copies. | These are automated regressions, not a commercial-game playback test. |

Paths in the table are beneath vita3k/ unless they explicitly start with ios/
or tools/. The Git comparison to a86b5f93 is the complete modification list.

## Local game selection

See local-game-versions.json for individually inspected metadata. Start with
the NoNpDrm PCSD00001 base 01.00 and its matching 01.03 patch, in that order.
The base has a 512-byte work.bin; its presence does not prove successful PFS
decryption. Keep licenses private. Do not concatenate the three packages.

The VPK's ZIP CRC passed, but its SFO is category gp (patch) version 01.03;
do not assume it is a standalone base. The MaiDump version is 01.00 and
contains modified modules, so keep it as a separate comparison. No game has
been booted on a desktop or phone during this development session.

## First phone test

1. Install the new Uncharted PSV IPA with SideStore. Select **Uncharted PSV**
   in StikDebug and enable JIT for this application.
2. Import required firmware and your complete NoNpDrm base, private license
   if requested, then matching patch. Set system language to traditional
   Chinese. Do not migrate the old application's shader/pipeline caches.
3. Launch PCSD00001. Record JIT status, library version, menu appearance and
   whether the first playable scene advances. Take the same character,
   building, sky and waterfall views as the old test for comparison.
4. Verify both sticks/buttons/touch, a scene transition and a second launch.
   Record approximate FPS and whether memory/temperature rises during ten
   minutes. This is an acceptance procedure, not an already passed result.

The user reported iOS 27.01; this has not been independently read from the
phone. The package deployment baseline is iOS 26.0.

## Evidence if rendering fails

Export the new application's tsubomi.log (the retained runtime's log name),
the iOS .ips crash report if any, and a screenshot/video with scene and time.
Keep the complete startup through the first wrong frame so the log includes
core revision, JIT/memory configuration, GPU feature state and shader version.
The watchdog records guest-thread/frame progress separately from GPU errors.

For pipeline failure, preserve the first full PIPELINE DUMP: both shader
hashes, format/raw attachment flags, blend/depth/stencil/render-pass state,
SPIR-V path and MoltenVK error. Share only the implicated shader/debug files
privately if further analysis requires them; never upload game archives,
firmware or licenses. Without a pipeline failure, obtain the character's
vertex/fragment shader hashes and a bounded surface/texture capture before
attributing the colour to the ALU or texture decoder. A screenshot alone is
insufficient to distinguish those causes.

Automated host tests do not replace on-device shader execution, Vulkan
resource validation, JIT verification or the final playable-game criterion.
