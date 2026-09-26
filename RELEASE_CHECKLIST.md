# Release checklist

## Automated checks completed locally

- [x] Plugin descriptor parses as valid JSON.
- [x] Public release version is `1.0.1`.
- [x] Game version range starts at build `502094`.
- [x] SML dependency is declared as `^3.12.0`.
- [x] Windows game client is selected as the verified first-release target.
- [x] The plugin has an original `Resources/Icon128.png` icon.
- [x] Source license and changelog are present.

## Checks required before making the SMR page public

- [ ] Install the final multi-target ZIP through a clean Satisfactory Mod Manager profile.
- [ ] Confirm F5-F8 default controls and remapping in a new save.
- [ ] Confirm calculator open/close, Escape handling, graph pan/zoom, and language switching.
- [ ] Confirm goal creation, reset, save/load, building placement, dismantle, and manual completion.
- [ ] Confirm host and client behavior in multiplayer.
- [ ] Install the Linux cross-compile toolchain and confirm Windows/Linux dedicated-server startup before adding dedicated-server targets.
- [x] Capture current in-game screenshots for the SMR description.
- [x] Create the SMR page with the permanent mod reference `SatisfactoryBuildCalculator`.
- [ ] Upload `SatisfactoryBuildCalculator.zip`, review compatibility, then publish.
