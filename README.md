# Starfrost Widgets

An SKSE plugin that puts three live HUD widgets on screen for
[Starfrost](https://www.nexusmods.com/skyrimspecialedition/mods/93732)'s survival
needs and [Blade & Blunt](https://www.nexusmods.com/skyrimspecialedition/mods/49015)'s
injuries:

| Widget | Icon | Source |
| --- | --- | --- |
| Hunger | drumstick | `Survival_HungerNeedValue` + the `Survival_HungerStage1..5Value` thresholds |
| Sleep | crescent moon | `Survival_ExhaustionNeedValue` + `Survival_ExhaustionStage1..5Value` |
| Injury | cracked heart | `MAG_InjurySpell01/02/03` on the player |
| Cold *(optional, off by default)* | snowflake | `Survival_ColdNeedValue` + `Survival_ColdStage1..5Value` |

Each widget grades from green through red across six severity stages, and can be
drawn as a ring gauge, a bare icon, or a bar. Everything is moveable in game.

## Moving things around

Press **Insert** (configurable) to enter edit mode. The player's controls are
parked, a software cursor appears, and every widget becomes a draggable box.
A panel gives you style, scale, opacity and per-axis position for each one, plus
a live readout of what the game is currently reporting.

Press **Insert** again or **Esc** to leave. Settings are written back to
`Data/SKSE/Plugins/StarfrostWidgets.ini` on the way out, so hand-editing the ini
and editing in game agree with each other.

Positions are stored as a fraction of the screen, so they survive a resolution
change.

## Requirements

- Skyrim SE/AE with SKSE64
- Creation Club Survival Mode
- Survival Mode Improved (for the exhaustion and cold stage thresholds, and the
  per-need enable switches)
- Blade & Blunt with `bEnableInjuries = true` — only needed for the injury widget

Missing any of these is not fatal: a widget whose forms cannot be resolved simply
does not draw, and the plugin logs which lookups failed to
`Documents/My Games/Skyrim Special Edition/SKSE/StarfrostWidgets.log`.

Skyrim VR is not supported — the overlay draws into the flat swap chain.

## How it works

Needs are read straight out of the Survival Mode / Survival Mode Improved global
variables rather than through Papyrus, so the widgets track the game with no
script latency. Forms are resolved by editor ID first, which means Starfrost's
overrides of the vanilla values are picked up automatically; the plugin + form ID
pair is only a fallback.

Drawing is Dear ImGui on a `IDXGISwapChain::Present` vtable hook. Input comes
from SKSE's own input events rather than a `WndProc` hook, which keeps the plugin
out of ENB's and ReShade's way and works despite Skyrim clipping the cursor.
Anything that touches game state — reading the player's spells, parking the
control map, writing the ini — is bounced onto the main thread through SKSE's
task queue.

## Building

```
git clone --recurse-submodules <this repo>
cmake --preset release
cmake --build --preset release
```

Needs `VCPKG_ROOT` set. Set `SKYRIM_MODS_FOLDER` to have the build drop the DLL,
PDB and ini straight into `<that folder>/StarfrostWidgets/SKSE/Plugins`.
