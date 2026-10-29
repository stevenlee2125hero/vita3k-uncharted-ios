# Full Plus iOS host

This directory contains only the real Vita3K-Plus host and its platform tests.
The bootstrap interpreter/clear-screen demo and its fixtures have been removed.
The supported application configuration is VITA3K_BUILD_IOS_UPSTREAM_CORE=ON.

Core: a86b5f93db43e4c4640ad1527e2a4695aad0f35b.
Runtime reference: myfin555-code/Vita3K-iOS 432342d2ef4f86ce98fad2b4362678d84cd4e204.
The historical PLUS_GRAPHICS_PORT.md is a comparison reference; this project
retains the complete Plus renderer and shader sources, not that partial port.

CI builds arm64 iPhoneOS with MoltenVK 1.4.2 and validates the unsigned IPA.
Bundle ID com.stevenlee.unchartedpsv; data Documents/UnchartedPSV.
Use SideStore to sign/install and select this new application in StikDebug.
Import firmware and the matching PCSD00001 base/patch. Do not import old caches.

No successful iPhone rendering, gameplay or frame-rate result is claimed yet.
