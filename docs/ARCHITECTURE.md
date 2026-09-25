# Architecture

This document describes the code at the level of layers and events. For the full specification, see [PLAN.md](PLAN.md).

```
 Views (axmol Nodes, built in code)          Controllers (singletons)                Pure logic (no axmol, unit-tested)
 ─────────────────────────────────           ───────────────────────────             ─────────────────────────────────
 LobbyScene / GameScene                      SceneController   (scene factory)
 BoardView, TokenView, DiceView  ──Ui*──▶    GameController ──────────────────▶      TurnMachine, Rules, BotBrain
 PlayerPanelView, ResultPopup    ◀─Game*──     paces events, drives bots             BoardQueries, BoardLayout
 MissionHudView, MissionCard     ◀─Mission*─ MissionController ───────────────▶      MissionEngine, MissionTracker
 CoinCounterView                 ◀─Wallet*─    (plug-in; game never sees it)         Conditions/Objectives registries
 DebugOverlayView (DEV)          ──Debug*──▶ WalletController (UserDefault)          Director: A* + Monte Carlo + utility
                                             ConfigController (JSON, live in DEV)    ConfigParser, MissionParser
```

## Rules

- **Views ↔ Controllers communicate only through events.** `EventBus` wraps axmol's `EventDispatcher` with typed structs, each carrying a compile-time `NAME`.
  - Views publish `Ui*` intents and never include `Controllers/`.
  - Controllers publish the state events that views need.
- **Models are data only.** Examples are `MatchState`, `GameEvent`, `MissionDef` and `GameConfig`.
- **All logic lives in Controllers.** The axmol-independent part sits in `Controllers/Logic/` and is covered by a doctest suite (`scripts/run_tests.sh`).
- **The game is unaware of missions.** `MissionController` subscribes to `GameEventMsg` and reads `GameController::matchState()`. Setting `missions.enabled = false` leaves a complete game.
- **Pacing.** `TurnMachine` resolves each command instantly. `GameController` then releases the resulting events one at a time, waiting after each for the matching animation length (from `game_config.json` timing, multiplied by `animScale`). Views use the same numbers, so animations and logic stay in step.
- **Missions are designer-owned JSON.** The files are `Content/config/missions.json` and `game_config.json`. New building blocks are one registered class each; see [MISSIONS_GUIDE.md](MISSIONS_GUIDE.md).

## Quirks of the axmol fork we build on

The fork lives in `./axmol`, copied from chaupar.

- **File lookup by basename only.** `FileUtils::fullPathForFilename` resolves files only through a cache keyed by basename, with no filesystem fallback. `AppDelegate` registers the bundled `Content` directory at boot. As a result, every file in `Content/` needs a unique basename, and `build_mac.sh` enforces this.
- **Renderer.** OpenGL is required (`AX_USE_COMPAT_GL ON`), because the fork's Metal backend is broken for sprites. `AX_ENABLE_3D` and `AX_ENABLE_PHYSICS` must stay on.
- **Extra pure virtuals.** `AppDelegate` must implement `applicationWillResignActive` and `applicationDidBecomeActive`.
- **DrawNode colours.** `DrawNode` takes `Color4B`.
