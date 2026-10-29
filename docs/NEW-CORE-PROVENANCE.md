# Independent Uncharted iOS port

This project starts at Vita3K-Plus `all-enhancements` commit
`a86b5f93db43e4c4640ad1527e2a4695aad0f35b`. It does not start from the
historical stevenlee2125hero Golden Abyss repair branches.

The iOS host, UIKit/Swift input, StikDebug/Oaknut JIT pool, build support,
and native tests are adapted from myfin555-code/Vita3K-iOS commit
`432342d2ef4f86ce98fad2b4362678d84cd4e204`. Existing copyright and
third-party license notices are retained. `ios-integration-paths.json`
records the selected integration paths; `ios-conflict-resolution.json`
records the initial three-way conflict decisions. Later compiler and test
repairs are separate commits and the final Git diff is authoritative.

The full Plus shader translator, compute reinterpretation, raw F16 paths,
depth/stencil lifetime and resource ownership remain the core. They are
not replaced with the partial graphics port in the iOS reference. The
cache version is isolated at 1001, with Bundle ID
`com.stevenlee.unchartedpsv` and Documents/UnchartedPSV. Old Tsubomi data
and shader caches are never migrated.

MoltenVK uses iOS-owned staging readback where direct guest/GPU memory
mapping is unavailable. The adaptation retains Plus format repacking and
tile layout, adds bounded partial-rectangle copies, transfer/host
visibility and post-fence allocation-lifetime validation. This requires
device verification; a compiled implementation is not a game success.

The unused bootstrap interpreter, clear-screen demo and bootstrap fixtures have been removed. Only the full upstream-core application target is supported and used in delivery CI.
The independent package is named Vita3K-Uncharted-iOS.ipa, with an embedded
source-provenance record. The IPA verifier checks arm64 iPhoneOS identity,
Bundle ID, minimum OS, CRC, full-core size, bundled SPIR-V and hashes.

The local commercial game files are outside this repository. The metadata
audit in local-game-versions.json contains no game, firmware, license or
key bytes. No device/desktop gameplay result is claimed by that audit.

No iPhone visual acceptance result exists yet. Native-resolution rendering,
character materials, UI, controller/touch, scene transitions, repeat launch
and sustained performance must be tested on the target device after a
successful complete build.
