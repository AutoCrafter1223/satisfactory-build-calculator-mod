# Changelog

## 1.0.1

- Added item icons to production cards, production summaries, and construction goals.
- Added hierarchical indentation to construction goals created from calculator branches.
- Added visible parent-child connector lines to hierarchical construction goals.
- Corrected connector placement and final-child endings using visible parent keys.
- Increased the brightness and thickness of the selected-goal outline.
- Calculator branches are now resolved and validated completely before any goals are created.
- Goal batches are committed atomically and report expected, added, and unresolved counts instead of silently skipping entries.
- Construction goals and building tracking are now volatile session data and are not written to save files.
- Completed goals are now hidden without deleting their tracking state.
- Added a toggle to hide locked products from the product list.
- Added an `Unlocked only` / `All recipes` selector beside the locked-product toggle; locked recipes remain unavailable by default.
- Improved compact-card readability by placing Select and Collapse on separate rows, reducing button padding, and spacing English rate units.
- Fixed Select and Collapse to the same width in every language and view mode, and increased the selected-card highlight visibility.
- Enlarged the fixed card-action buttons and explicitly centered their labels to prevent translated text clipping.
- Kept detailed machine counts and power usage on one line with shortened labels, ellipsized building names, and full tooltips.
- Replaced misleading English `installed` labels with `build` in calculator cards.
- Fixed the empty construction-goal instruction so it follows the active game language immediately.
- Updated rate units to `개/분` and `m³/분` in Korean, and `item/min` and `m³/min` in English.

## 1.0.0 - Initial public release candidate

- Added an in-game production calculator with connected process cards.
- Added construction goals with manual and automatic progress tracking.
- Added recipe synchronization for the current save and compatible installed mods.
- Added generator and fuel calculations.
- Added configurable F5-F8 hotkeys.
- Added Korean, English, Simplified Chinese, and German interface translations.
- Added detailed and compact card layouts, graph panning, and wheel zoom.
- Added per-player save data and multiplayer remote-call support.
