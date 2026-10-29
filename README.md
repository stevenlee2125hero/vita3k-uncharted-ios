# Vita3K Uncharted iOS

Independent PCSD00001-focused iOS port starting from the full Vita3K-Plus
all-enhancements core a86b5f93db43e4c4640ad1527e2a4695aad0f35b.

The iOS runtime is separately adapted from myfin555-code/Vita3K-iOS
432342d2ef4f86ce98fad2b4362678d84cd4e204. See docs/NEW-CORE-PROVENANCE.md.
This is not based on the historical Golden Abyss repair branches.

GitHub Actions builds the complete arm64 emulator as Vita3K-Uncharted-iOS.ipa,
unsigned for SideStore. Enable StikDebug JIT before game launch.
App Bundle ID: com.stevenlee.unchartedpsv. Data: Documents/UnchartedPSV.
No old Tsubomi data or shader caches are migrated.

No games, firmware or keys are distributed. Local game metadata audit:
docs/local-game-versions.json. Preferred test candidate is the NoNpDrm
base 01.00 plus matching 01.03 patch; decryption and gameplay still require
validation. The separately named 01.03 VPK identifies itself as a patch.

Build, shader tests, and actual iPhone rendering are separate acceptance gates.
No successful iPhone gameplay is currently claimed.

License: GPL-2.0-or-later; existing third-party notices are retained.
