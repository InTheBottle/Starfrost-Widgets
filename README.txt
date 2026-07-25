Starfrost Widgets 1.0.0
by bottle

Live HUD widgets for Starfrost's survival needs and Blade & Blunt's injuries.
Three gauges - Hunger, Sleep and Injury - sit on the HUD and colour-grade from
green to red as things get worse. A fourth, Cold, is included but switched off.
Everything can be dragged into place in game.

REQUIREMENTS
  - Skyrim Special Edition / Anniversary Edition
  - SKSE64
  - Address Library for SKSE Plugins
  - Creation Club Survival Mode
  - Survival Mode Improved
  - Blade & Blunt, with bEnableInjuries = true (only for the Injury widget)

  Skyrim VR is not supported.

  Nothing here is fatal if missing. A widget whose forms cannot be found simply
  does not draw, and the log names the lookups that failed.

INSTALL
  Mod manager (recommended):
    Add this archive in Mod Organizer 2 or Vortex and enable it. The archive is
    already rooted at the Data folder, so no manual repackaging is needed.

  Manual:
    Copy the SKSE folder into your Skyrim Data folder, merging with what is
    already there:
      Data\SKSE\Plugins\StarfrostWidgets.dll
      Data\SKSE\Plugins\StarfrostWidgets.ini

  StarfrostWidgets.pdb is optional. It is only used by crash loggers to produce
  readable stack traces, and can be deleted.

  No ESP/ESL plugin and no scripts. Safe to add or remove from a save at any
  time.

MOVING THE WIDGETS
  Press Insert in game. Your controls are parked, a cursor appears, and every
  widget becomes a box you can drag. A panel opens alongside them with style,
  scale, opacity and per-axis position for each widget, plus a live readout of
  what the game is currently reporting.

  Press Insert again, or Esc, to leave. Settings are written back to the ini on
  the way out, so hand-editing the file and editing in game agree with each
  other.

  Positions are stored as a fraction of the screen, so they survive a
  resolution change.

SETTINGS
  Data\SKSE\Plugins\StarfrostWidgets.ini is read once when the game starts, so
  hand edits need a restart. Edits made in game apply immediately.

  [General]
    bEnabled              Master switch for every widget.
    iEditModeKey          DirectInput scan code that opens edit mode.
                          210 = Insert. 88 = F12, 87 = F11, 68 = F10.
    fGlobalScale          Multiplies every widget's own scale. 0.25 - 4.0.
    fOpacity              0.05 - 1.0.
    bHideInMenus          Hide while a menu or dialogue is open.
    bHideWhenHUDHidden    Follow the game's own HUD visibility, so screenshots
                          taken with the HUD off stay clean.
    bRequireSurvivalMode  Only show the need widgets while Survival Mode is on.
                          Injuries ignore this - they are Blade & Blunt's, not
                          Survival Mode's.
    bShowValues           Print the raw need value under each widget.
    bPulseAtCritical      Pulse a widget at its highest severity stage.
    fPollInterval         Seconds between reads of the game's need values.

  [Hunger] [Sleep] [Injury] [Cold]
    bEnabled              Show this widget.
    fPosX, fPosY          Position as a fraction of screen size, 0.0 - 1.0,
                          measured to the widget's top-left corner.
    fScale                0.25 - 4.0.
    iStyle                0 = ring gauge, 1 = icon only, 2 = bar.
    bHideWhenSatisfied    Hide entirely while the need is at stage 0.
    sStage0Color ...      Six RRGGBB colours, stage 0 (satisfied) through
    sStage5Color          stage 5 (critical). Injuries have four states, so
                          they use stages 0, 2, 4 and 5.

  Cold is off by default because Starfrost already gives it a vanilla HUD
  meter. Set bEnabled = true under [Cold] if you would rather use this one.

VERIFYING IT LOADED
  After launching the game, check:
    Documents\My Games\Skyrim Special Edition\SKSE\StarfrostWidgets.log
  A working load looks like:
    info: StarfrostWidgets loaded
    info: Loaded settings from Data/SKSE/Plugins/StarfrostWidgets.ini
    info: Form resolution: hunger=true sleep=true cold=true injuries=true
    info: Input sink installed
    info: Present hook installed
    info: ImGui initialised

  A false in that form resolution line means the matching mod is not installed
  or is not where the plugin expected it, and that widget will stay hidden.
  Warnings just above it name the exact records that could not be found.

  If the log does not exist, SKSE is not loading the plugin - confirm you
  launched through skse64_loader.exe and that the DLL is in Data\SKSE\Plugins.

NOTES
  - Need values are read straight from the Survival Mode / Survival Mode
    Improved global variables rather than through Papyrus, so the widgets track
    the game with no script latency.
  - Records are found by editor ID first, so Starfrost's overrides of the
    vanilla thresholds are picked up automatically.
  - Drawing is done on the swap chain's Present call, and input comes from
    SKSE's own input events rather than a window hook. That keeps this out of
    ENB's and ReShade's way.

UNINSTALL
  Disable in your mod manager, or delete the files listed above.
