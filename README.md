# Satisfactory Build Calculator

In-game production calculator and construction goal tracker for Satisfactory.

## Installation

The public release is intended to be installed through Satisfactory Mod Manager. SMM installs the compatible Satisfactory Mod Loader dependency and places the mod files correctly. Manual installation is intended only for development and troubleshooting.

The mod does not collect telemetry, contact external services, or modify files outside Satisfactory's normal mod and save-data paths.

## Current controls

- `F5`: toggle manual completion for the selected goal
- `F6`: add the production building under the crosshair as a goal template
- `F7`: show or hide the compact goal HUD
- `F8`: open or close the production calculator
- `Up` / `Down`: select a goal
- `+` / `-`: increase or decrease the selected target count
- `L`: manually link the building under the crosshair to the selected goal
- `Delete`: remove the selected goal

The calculator closes with `Escape` without intentionally opening the pause menu.
All four function-key shortcuts can be changed from the mod's in-game configuration page. Press the displayed key button, then press the replacement key.

## Calculator

- Synchronizes the current save's unlocked recipes whenever it opens.
- Keeps locked products visible with a text `Locked` badge.
- Imports standard manufacturing recipes registered by other installed mods at runtime.
- Supports per-card alternate recipes, Power Shards, and Somersloops.
- Presents the chain as a connected parent-child card graph with building-specific accent colors.
- Drag empty graph space with the left mouse button to pan; use the wheel to zoom around the pointer.
- Provides reset-view and focus-selected-card controls, plus detailed and compact card views with text expand/collapse controls.
- Summarizes machine count, raw resources, power consumption, and generation.
- Selecting a card and pressing the goal button adds only that card and its descendants to construction goals.
- The adjacent clear button removes only the current player's goals after a second confirmation click.

The calculator UI supports Korean, English, Simplified Chinese, and German, selected from the current in-game language. Item, recipe, and building names use Satisfactory's own localized display names; unsupported UI languages fall back to English.
Its two-line shortcut guide stays fixed while the goal list scrolls, and keyboard selection is visibly highlighted.

## Tracking rules

- Goals are stored per player in the save.
- Newly constructed buildings are counted only when their building class, recipe, Power Shard count, and Somersloop count match.
- Dismantling a linked building decreases progress.
- Recipe or enhancement changes cause the building to be reassigned.
- Existing buildings are ignored until the player explicitly links them with `L`.
- Manual completion remains complete even if automatic progress later changes.
- Ambiguous automatic matches wait for the player to link the building to the intended selected goal.

## Development status

Version `1.0.0` is the first public release candidate. The native runtime, save/replication layer, multiplayer remote calls, automatic tracker, compact HUD, configurable hotkeys, and production calculator compile for `FactoryGameSteam` Shipping.

Built against Satisfactory build `502094` and Satisfactory Mod Loader `3.12.0`. The first public package targets the Windows game client, including normal single-player and listen-server play. Dedicated-server packages are intentionally omitted until separate Windows and Linux server testing is complete.

## Support and source

- Source: https://github.com/gyeong-seog/satisfactory-build-calculator
- Issues: https://github.com/gyeong-seog/satisfactory-build-calculator/issues
- License: MIT
