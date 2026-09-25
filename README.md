# Satisfactory Build Calculator Mod

Early in-game goal tracking prototype for Satisfactory.

## Current controls

- `F8`: show or hide the compact goal HUD
- `F7`: add the production building under the crosshair as a goal template
- `Up` / `Down`: select a goal
- `+` / `-`: increase or decrease the selected target count
- `Enter`: toggle manual completion
- `L`: manually link the building under the crosshair to the selected goal
- `Delete`: remove the selected goal

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

The native runtime, save/replication layer, multiplayer remote calls, automatic tracker, and compact HUD compile for both `FactoryEditor` Development and `FactoryGameSteam` Shipping. Input is temporarily handled by fixed prototype keys; player-rebindable Enhanced Input assets are planned before public release.
