# Satisfactory Build Calculator Mod

In-game production calculator and construction goal tracker for Satisfactory.

## Current controls

- `F8`: show or hide the compact goal HUD
- `F6`: open or close the production calculator
- `F7`: add the production building under the crosshair as a goal template
- `Up` / `Down`: select a goal
- `+` / `-`: increase or decrease the selected target count
- `Enter`: toggle manual completion
- `L`: manually link the building under the crosshair to the selected goal
- `Delete`: remove the selected goal

The calculator closes with `Escape` without intentionally opening the pause menu.

## Calculator

- Synchronizes the current save's unlocked recipes whenever it opens.
- Keeps locked products visible with a text `Locked` badge.
- Imports standard manufacturing recipes registered by other installed mods at runtime.
- Supports per-card alternate recipes, Power Shards, and Somersloops.
- Offers detailed and compact card views.
- Summarizes machine count, raw resources, power consumption, and generation.
- Adds the calculated manufacturing chain to construction goals with one button.

The HUD automatically uses Korean when the game culture is Korean. Other cultures use English.

## Tracking rules

- Goals are stored per player in the save.
- Newly constructed buildings are counted only when their building class, recipe, Power Shard count, and Somersloop count match.
- Dismantling a linked building decreases progress.
- Recipe or enhancement changes cause the building to be reassigned.
- Existing buildings are ignored until the player explicitly links them with `L`.
- Manual completion remains complete even if automatic progress later changes.
- Ambiguous automatic matches wait for the player to link the building to the intended selected goal.

## Development status

The native runtime, save/replication layer, multiplayer remote calls, automatic tracker, compact HUD, and production calculator compile for `FactoryGameSteam` Shipping. Input is temporarily handled by fixed prototype keys; player-rebindable Enhanced Input assets are planned before public release.
