Starfrost Widgets 1.1.0
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

  Ctrl+click any slider to type an exact value instead of dragging for it.

  Press Insert again, or Esc, to leave. Settings are written back to the ini on
  the way out, so hand-editing the file and editing in game agree with each
  other.

  While edit mode is open the game sees no keyboard or mouse input at all, so
  typing into a field cannot fire a hotkey or swing a weapon by accident.

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
    iRenderTarget         Where the overlay draws. See FRAME GENERATION below.
                          0 = auto, 1 = swap chain, 2 = game framebuffer.

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

FRAME GENERATION
  Frame generation - Community Shaders' Upscaling feature, or any of the FSR3 /
  DLSS-G mods - swaps the game's swap chain for one of its own and composites
  the finished image itself. Anything drawn straight into the back buffer is
  overwritten before it ever reaches the screen, which in 1.0.0 meant the
  widgets vanished or flickered as soon as frame generation was switched on.

  The overlay now draws into the game's framebuffer render target instead. That
  is the same layer the vanilla HUD uses, so the compositor picks it up - and
  frame generation leaves that layer alone rather than interpolating it, so the
  widgets stay sharp instead of smearing between frames.

  iRenderTarget picks the surface:

    0  Auto (default)     Use the game's framebuffer when it can be found, and
                          fall back to the back buffer if it cannot. Correct
                          with or without frame generation; leave it here.
    1  Swap chain         Always the back buffer. This is what 1.0.0 did. Only
                          worth trying if the widgets misbehave with frame
                          generation switched off.
    2  Game framebuffer   Always the game's framebuffer, with no fallback. If it
                          cannot be resolved, nothing draws and the log says so.

  There is also a "Draw into" dropdown in the edit mode panel, so you can flip
  between them in game and watch which one works.

  The log records the surface it settled on, and again if it ever changes:
    info: Drawing into the game framebuffer (2560x1440)

VERIFYING IT LOADED
  After launching the game, check:
    Documents\My Games\Skyrim Special Edition\SKSE\StarfrostWidgets.log
  A working load looks like:
    info: StarfrostWidgets loaded
    info: Loaded settings from Data/SKSE/Plugins/StarfrostWidgets.ini
    info: Form resolution: hunger=true sleep=true cold=true injuries=true
    info: Input poll hook installed
    info: Present hook installed
    info: ImGui initialised
    info: Drawing into the game framebuffer (2560x1440)

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

CHANGES IN 1.1.0
  - Sliders now take typed input. Ctrl+click one and enter the number you
    actually want. ImGui has always supported this; what was missing was a
    keyboard - the overlay only ever received the mouse, so the text box opened
    with no way to type into it.
  - Edit mode now takes input away from the game at the source rather than
    switching player controls off. Nothing reaches the game while the panel is
    up, so typing a digit cannot equip a hotkeyed item, and there is no control
    state left disabled if the game goes down mid-edit.
  - Works with frame generation. The overlay now draws into the game's
    framebuffer render target rather than the swap chain back buffer, so the
    frame generation compositor no longer overwrites the widgets. Added
    iRenderTarget, and a "Draw into" dropdown in the edit panel, for anyone who
    needs to force one or the other.
  - The device and context now come from the game's renderer rather than from
    the swap chain, which is not necessarily a real D3D11 swap chain once frame
    generation has replaced it.
  - Widget geometry is measured against the surface actually being drawn into
    rather than whatever size the swap chain reports, so positions and the edit
    mode cursor stay aligned when those differ.

UNINSTALL
  Disable in your mod manager, or delete the files listed above.
