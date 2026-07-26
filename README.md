# Starfrost Widgets

An SKSE plugin that puts live HUD widgets on screen for
[Starfrost](https://www.nexusmods.com/skyrimspecialedition/mods/93732)'s survival
needs, [Blade & Blunt](https://www.nexusmods.com/skyrimspecialedition/mods/49015)'s
injuries, [Stress and Fear](https://www.nexusmods.com/skyrimspecialedition/mods/116522)'s
stress, and the timed buffs from
[Gourmet](https://www.nexusmods.com/skyrimspecialedition/mods/60063) and
[Pilgrim](https://www.nexusmods.com/skyrimspecialedition/mods/45557).

### Needs

| Widget | Icon | Source |
| --- | --- | --- |
| Hunger | drumstick | `Survival_HungerNeedValue` + the `Survival_HungerStage1..5Value` thresholds, or Starfrost's own hunger abilities — see below |
| Sleep | crescent moon | `Survival_ExhaustionNeedValue` + `Survival_ExhaustionStage1..5Value` |
| Injury | cracked heart | `MAG_InjurySpell01/02/03` on the player |
| Stress | skull | Stress and Fear's `Stress_Total`, gated on `Stress_Enabled` |
| Cold *(optional, off by default)* | snowflake | `Survival_ColdNeedValue` + `Survival_ColdStage1..5Value` |

These grade from green through red across six severity stages as the need climbs.

#### Stress

Stress and Fear keeps one 0-100 total in `Stress_Total` and hangs four debuff
abilities off it, each conditioned on a band of that number: Minor Combat Stress
above 25, Moderate above 45, Severe above 65, Critical above 85. The widget reads
the global and uses those same four thresholds, so the colour it draws in changes
at exactly the point the game hands you the next debuff. Stage 1 is the gap below
25 — stress you are carrying that has not cost you anything yet — which means the
whole six-colour ramp gets used and `bDynamic` with **Show from stage** 2 hides
the widget until a debuff is actually live.

Turning the stress system off in the mod's MCM sets `Stress_Enabled` to 0 and
zeroes the total, so the widget watches that global too rather than sitting at a
permanent, meaningless calm.

Fears are not covered. They are per-enemy-type flags that only matter while you
are fighting that enemy, so there is no gauge to draw.

#### Which hunger

Starfrost has shipped two different hunger systems and the widget handles both
with no configuration.

Builds that keep Creation Club Survival Mode's hunger meter are read straight off
`Survival_HungerNeedValue`, and the ring fills smoothly.

Starfrost's public Nexus build replaced that meter with its own **Hungry** /
**Very Hungry** / **Famished** abilities and sets `SMI_HungerShouldBeEnabled` to
0, so the need value never moves and the widget would stay hidden however hungry
you got. Those abilities only exist on that build, so their presence is the test:
find them and the widget reads the ability tier instead. It steps in thirds rather
than filling smoothly, because the countdowns between tiers are Papyrus game-time
timers with nothing readable to expose.

The log records which one it settled on.

### Buff timers

| Widget | Icon | Source |
| --- | --- | --- |
| Food | steaming bowl of soup | Gourmet's `MAG_FoodFortify{Health,Magicka,Stamina}Regen{Basic,Marriage}`, plus `Survival_FoodFortifyWarmth` |
| Alcohol | mead bottle | Gourmet's `MAG_AlcoholFortify{Magicka,Stamina}` and their paired drains |
| Blessing | Shrine of Mara medallion | anything carrying Pilgrim's `MAG_PilgrimShrineBlessing` / `MAG_CultistShrineBlessing` keyword |

These read the same ramp backwards: the ring starts full and green when the buff
lands and empties towards red as it runs out, so the pulse at the last stage is a
warning that it is about to drop. The remaining time is printed underneath.

With **Dynamic** off they hold their spot on the HUD while nothing is running,
drawn as a dim grey icon on an empty ring with no timer under it, so the layout
does not shuffle every time a buff lands or expires. A widget whose source mod is
not installed at all still draws nothing.

Because a single food effect does not tell you the whole story, the food and
alcohol widgets carry a badge row for what is currently fortified — a red circle
for health, a blue diamond for magicka, a green triangle for stamina, an orange
square for warmth. Eat a Homecooked Meal and the first three all light up.

Warmth is in there because Gourmet's survival stews grant Survival Mode's own
`Survival_FoodFortifyWarmth` on the same twenty-minute timer, and a few of them
grant nothing else — without it those bowls would show no food timer at all.

The blessing widget covers all 45 of Pilgrim's blessings, Aedric and Daedric
alike, whether you took them at a shrine or by praying. Each blessing grants a
different boon, but every one of those boons carries a shrine-blessing keyword,
so there is no per-deity list to keep in sync. Which blessing is running is named
in the edit panel.

Matching the keyword rather than the `MAG_PilgrimXPEffect` / `MAG_CultistXPEffect`
markers the blessings also share is deliberate. Those markers are gated behind
Pilgrim's anti-farming XP cooldown, so praying re-casts the blessing without them
and a widget keyed on them would show nothing. The boons themselves carry no
conditions at all.

Every widget can be drawn as a ring gauge, a bare icon, or a bar, and everything
is moveable and recolourable in game.

## Moving things around

Press **Insert** (configurable) in game to enter edit mode. The player's controls
are parked, a software cursor appears, and every widget becomes a draggable box.
It only opens where there is a HUD to arrange, so the key does nothing on the main
menu or a loading screen, where it would park your controls with no way to see it.
A panel gives you style, scale, opacity, per-axis position and a colour picker
for each of the six stages, plus a live readout of what the game is currently
reporting.

The six swatches under each widget are what the icon, ring and bar are drawn in
at each severity stage. Set all six to the same colour if you would rather a
widget did not change colour at all; "Copy to all widgets" pushes one widget's
ramp onto the rest.

## Dynamic widgets

Tick **Dynamic** on a widget and it stays off the HUD until it has something
worth saying. **Show from stage** picks how bad things have to get first, 1
through 5 — the same scale as the colour swatches sitting just below it, so you
can see what it will look like when it does turn up.

On the needs and injuries that means it appears as things get worse. On the buff
timers the ramp runs backwards, so it means it appears as the buff runs down: a
Blessing widget set to stage 4 stays hidden for most of its eight hours and shows
up when it is nearly gone, and it goes away again once the buff expires.

Untick it and the widget is on screen from the moment the game loads, whatever it
has to say.

Every widget still draws while edit mode is open regardless, so a hidden one can
always be repositioned.

This replaces `bHideWhenSatisfied`, which was the same idea with the threshold
nailed to stage 1. An older ini that still has that key is read as `bDynamic`.

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
- Stress and Fear — A Dynamic Sanity System — only needed for the stress widget
- Gourmet — A Cooking Overhaul — only needed for the food and alcohol widgets
- Pilgrim — A Religion Overhaul — only needed for the blessing widget

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

The buff timers walk the player's active effect list and match each entry's base
effect against the handful of forms above, taking `duration - elapsedSeconds` from
the longest runner. That is the same number the vanilla Active Effects menu shows,
and it picks up Gourmet's duration perks for free — The Art of Cooking triples the
food timer and the widget just sees a longer duration.

Drawing is Dear ImGui on a `IDXGISwapChain::Present` vtable hook. Input comes
from SKSE's own input events rather than a `WndProc` hook, which keeps the plugin
out of ENB's and ReShade's way and works despite Skyrim clipping the cursor.
Anything that touches game state — reading the player's spells, parking the
control map, writing the ini — is bounced onto the main thread through SKSE's
task queue.

The render target the game had bound is saved and restored around the ImGui pass.
ImGui's own state backup happens inside `RenderDrawData`, which is after the
overlay has already rebound the output merger, so its restore would only put the
overlay's target back rather than the game's. Skyrim's renderer caches what it
believes is bound and skips redundant binds, so leaving the wrong target there
sends whatever draws next into the wrong surface — which is how a HUD overlay
ends up breaking an unrelated mod's Scaleform widgets.

Menu visibility is tracked from `MenuOpenCloseEvent` on the main thread and
published as an atomic, rather than sampling the menu stack from the render
thread. A menu counts as covering the screen if it pauses, goes modal, takes the
cursor or takes the menu input context, which catches custom menus from other
mods as well as the vanilla ones. Always-open menus — the HUD, the cursor, the
faders, other mods' widget layers — are ignored, or nothing would ever draw.

## Building

```
git clone --recurse-submodules <this repo>
cmake --preset release
cmake --build --preset release
```

Needs `VCPKG_ROOT` set. Set `SKYRIM_MODS_FOLDER` to have the build drop the DLL,
PDB and ini straight into `<that folder>/StarfrostWidgets/SKSE/Plugins`.
