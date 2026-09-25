# AGENTS.md — Ludo Missions

Hackathon MVP: a minimal Ludo game (1 human vs 3 bots) on the axmol engine with a JSON-driven
**in-game missions** system that awards coins. The full spec is in [docs/PLAN.md](docs/PLAN.md). Read §1–§9
and §11 before touching code, and implement **one phase at a time**.

## Layout
- `Source/Models/` — PURE DATA (structs/enums). No axmol, no logic.
- `Source/Controllers/Logic/` — PURE C++20 logic (rules, turn machine, bots, mission engine, director). No axmol includes. Unit-tested.
- `Source/Controllers/` — axmol adapter singletons (`sharedController()`): own models, run Logic, pace time, publish/subscribe events.
- `Source/Events/` — `EventBus` (typed wrapper over axmol `EventDispatcher`) + event structs.
- `Source/Views/` — axmol Nodes/Scenes built in code. **Never include `Controllers/`.** Talk to controllers only by publishing `Ui*` events.
- `Source/Utils/` — pure helpers (Log, JsonUtils).
- `Content/` — runtime assets + `config/game_config.json` + `config/missions.json` (designer-owned).
- `tests/` — standalone doctest project (does NOT build the engine).
- `axmol/` — engine copy (gitignored, created by `scripts/setup_engine.sh`). **Never edit.**
- `chaupar/` — Ludo Star reference repo (gitignored). Read-only reference, never commit.

## Commands
```sh
scripts/setup_engine.sh   # once: copy engine from chaupar/axmol
scripts/run_tests.sh      # headless unit tests (fast)
scripts/build_mac.sh      # configure + build macOS app (re-run after adding files: globs)
scripts/run_mac.sh        # build + run with logs in terminal
```

## Rules
- Includes are relative to `Source/`, e.g. `#include "Controllers/Logic/Rules.h"`.
- Game code never references missions. Missions listen to game events (`missions.enabled=false` must still play).
- `EventBus::subscribe<T>(this, ...)` needs the explicit `<T>`; unsubscribe with `EventBus::unsubscribeAll(this)`
  (BaseView/BaseScene do it in `onExit`). Never keep event payload pointers. Fixed priority is 1, never 0.
- Publish "ready" events only from `afterEnter()`, never from constructors or `init()`.
- Never switch the renderer to Metal (`AX_USE_COMPAT_GL ON` is required). Keep `AX_ENABLE_3D` and `AX_ENABLE_PHYSICS` ON.
- DrawNode in this fork takes `Color4B`.
- Scheduler one-shots: `unschedule(key, this)` before `schedule(...)`, guard callbacks with a match id.
- Formatting: clang-format (`.clang-format`, tabs width 4, column limit 150).
- The window/resolution rules for missions live only in `MissionTracker`.
