# Ludo Missions — Hackathon MVP Implementation Plan

## 0. Context

We are building **in-game missions** for Ludo: short objectives generated from the current board state, e.g. "Capture an enemy in the next 3 turns" or "Roll a 6 this turn". Completing a mission earns **coins**. `chaupar/` (Ludo Star, the shipping game) is **reference only**. We build our own small Ludo game from scratch, using the same patterns:
- MVC with an event bus.
- **All logic in Controllers.**
- **Models are plain data.**
- **Views and Controllers talk only through axmol custom events, in both directions.**

We reuse a minimal set of chaupar PNGs, fonts and sounds. Missions must be **designer-first**: a designer adds or tunes a mission by editing JSON, with no C++ changes.

The plan is split into small phases so that a lower-tier model can finish each one end to end. Every phase lists the exact files, the interfaces, how to check it passed, and the pitfalls.

---

## 1. Locked decisions (from the grilling session)

| Topic | Decision |
|---|---|
| Players | 1 human (RED, bottom-left) vs 3 heuristic bots (GREEN, YELLOW, BLUE). Turn order 0→1→2→3. |
| Platforms | macOS (primary dev and demo) + iOS (final phase) |
| Engine | chaupar's in-house **axmol 2.2.1 fork**, **copied** into `./axmol` (gitignored) by `scripts/setup_engine.sh` |
| Layout | Game at repo root. `chaupar/` stays local and is never committed. |
| Views | Built in C++ code from chaupar PNGs. No Cocos Creator, no `.ccreator` or `.csb` files. |
| Resolution | Portrait 720x1280, `ResolutionPolicy::FIXED_WIDTH`, Mac window 405x720 |
| V↔C | **Events in both directions.** Views never include Controller headers. |
| Rules | Standard Ludo Star rules (§5) with **STACK_AND_MOVE** sixes and **no blocks**. The match ends when the first player finishes all 4 tokens. |
| Timer | None for the human. A single legal move (1 token, 1 value) is played automatically. |
| Roll pick | Chaupar-style value chips pop up above a tapped token when more than one roll value is legal for it |
| Mission flow | Checked against board state. Up to **N=3** active at once, no duplicate id, cooldown per mission. **Accepted automatically**, with a banner. Failing costs nothing. Active missions are voided silently at match end. |
| Moments | Offers are evaluated at **turnStart** (the human's turn begins) and at **afterRoll** (the human's dice result is in and they must now move). Each mission lists its allowed `moments`. Caps: 1 offer per moment and 2 per human turn. |
| Serving algorithm | **Mission Director** (§7.7), which runs in three stages: (1) an **A\* feasibility search** (best-case dice, frozen bots) drops missions that can't finish inside their window; (2) **Monte Carlo rollouts** with the real TurnMachine + BotBrain estimate P(success); (3) **utility** = weight × difficulty fit × timeliness × novelty. The result is sampled with softmax, and there is a minimum-utility gate ("no offer is better than a bad offer"). **Adaptive difficulty band**: completions make it harder, failures make it easier, falling behind makes it easier. It is a switchable strategy; the fallback is weighted random. |
| Turn unit | One human turn (bonus rolls stay inside it). `turnsLeft` goes down at the **end of each human turn**; when it reaches 0 the mission resolves right then. The window includes the turn in which the mission was offered. |
| Mission shapes | `count` (one-shot is `target: 1`), `sum`, `streak`, `avoid`, `state` |
| Authoring | `Content/config/missions.json` made of structured JSON building blocks. Each block type is one C++ class in a registry. Mission choice is weighted random. |
| Tuning | `Content/config/game_config.json` holds rules toggles, timings, mission globals and debug settings |
| Coins | Missions are the only source. Saved in `UserDefault`. Screens: Lobby → Game → Result popup. |
| Tooling | Headless unit tests (doctest, a separate CMake project with no engine). DEV keys: 1–6 force the next roll, R reloads JSON, F toggles fast bots. |

---

## 2. Glossary (use these words in code; they avoid chaupar's confusing "home"/"goal")

- **Yard**: a player's corner base where tokens wait. Progress `-1`.
- **Track**: the shared 52-cell loop. A token's progress `0..50` maps to a global cell.
- **Home lane**: the 5 coloured cells leading to the centre. Progress `51..55`.
- **Finished**: the token reached the centre. Progress `56`.
- **Global cell**: `0..51`, an index on the track that is the same for every colour.
- **Self**: the human player, as seen by the mission engine.
- **Pending rolls**: the stack of dice values not yet used this turn (STACK_AND_MOVE).

---

## 3. Architecture

### 3.1 Layers and dependency rules (enforce in review)

```
Source/
  Models/              PURE DATA. Structs and enums only, plus trivial helpers. No axmol, no logic beyond getters.
  Controllers/Logic/   PURE C++20 LOGIC. No axmol includes. Unit-tested headlessly.
  Controllers/         axmol ADAPTERS (singletons). Own models, run Logic, pace time, publish/subscribe events.
  Events/              EventBus + event structs (the only thing Views and Controllers share besides Models)
  Views/               axmol Nodes/Scenes. Subscribe to events and render. Publish Ui* intents. Never include Controllers/.
  Utils/               Pure helpers (Log, JsonUtils over rapidjson)
```

Allowed includes:
- Views → Events, Models, Utils, Views.
- Controllers → Controllers/Logic, Events, Models, Utils, and other Controllers **read-only**. MissionController may read GameController's MatchState; GameController must **never** include Mission* headers.
- Logic → Models, Utils, Logic.
- Models → Models.

**Modularity rule:** the game knows nothing about missions. Missions are a plug-in that listens to game events. Setting `missions.enabled=false` must leave a fully working game.

### 3.2 Runtime flow

```
View (tap) --UiTokenTapped--> GameController --TurnMachine.move()--> [GameEvent...] --paced queue-->
    publish GameEventMsg (one bus channel)  -->  Views animate
                                            -->  MissionController -> MissionEngine.onEvent()
                                                   --> publish MissionUpdated --> MissionHudView
                                                   --> (COMPLETED) WalletController.add() --> publish WalletChanged --> CoinCounterView
```

### 3.3 Final folder tree

```
Ludo-Missions/
  .gitignore  AGENTS.md  CLAUDE.md  CMakeLists.txt  README.md
  axmol/                         (copied engine, gitignored)
  chaupar/                       (reference, gitignored)
  proj.ios_mac/{mac,ios}/        (from axmol template, edited)
  scripts/ setup_engine.sh build_mac.sh run_mac.sh run_tests.sh build_ios.sh run_ios_sim.sh copy_assets.sh
  Content/
    config/game_config.json  config/missions.json
    fonts/  images/{board,tokens,dice,ui}/  sounds/
  Source/
    AppDelegate.{h,cpp}
    Events/  EventBus.{h,cpp}  UiEvents.h  GameEvents.h  MissionEvents.h  WalletEvents.h  DebugEvents.h  AppEvents.h
    Models/  Types.h  BoardLayout.{h,cpp}  GameConfig.h  MatchState.h  GameEvent.{h,cpp}  MoveOption.h
             Params.h  MissionDef.h  MissionInstance.h  MissionUpdate.h
    Controllers/
      Logic/ Rules.{h,cpp}  TurnMachine.{h,cpp}  BoardQueries.{h,cpp}  BotBrain.{h,cpp}  Rng.h  ConfigParser.{h,cpp}
             Missions/ MissionParser.{h,cpp}  MissionEngine.{h,cpp}  EventFilter.{h,cpp}
                       Condition.h  ConditionRegistry.{h,cpp}  BuiltinConditions.cpp
                       Objective.h  ObjectiveRegistry.{h,cpp}  BuiltinObjectives.cpp  TextTemplate.{h,cpp}  MissionTracker.{h,cpp}
                       Director/ IOfferStrategy.h  WeightedRandomStrategy.*  DirectorStrategy.*  FeasibilitySearch.*
                                 RolloutSimulator.*  UtilityScorer.*  DifficultyTracker.*
      ConfigController.{h,cpp}  WalletController.{h,cpp}  SceneController.{h,cpp}
      GameController.{h,cpp}  MissionController.{h,cpp}
    Views/
      Common/ BaseView.{h,cpp}  BaseScene.{h,cpp}  UiConfig.h  UiFactory.{h,cpp}  CoinCounterView.{h,cpp}  ToastView.{h,cpp}
      Lobby/  LobbyScene.{h,cpp}
      Game/   GameScene.{h,cpp}  BoardGeometry.{h,cpp}  BoardView.{h,cpp}  TokenView.{h,cpp}  DiceView.{h,cpp}
              PlayerPanelView.{h,cpp}  RollChoiceView.{h,cpp}  ResultPopup.{h,cpp}  DebugOverlayView.{h,cpp}
      Missions/ MissionHudView.{h,cpp}  MissionCardView.{h,cpp}
    Utils/   Log.h  JsonUtils.{h,cpp}
  tests/  CMakeLists.txt  test_main.cpp  TestHelpers.h  *Tests.cpp
  docs/   MISSIONS_GUIDE.md  ARCHITECTURE.md
```

Namespace: everything is in `namespace lm`. App name `LudoMissions`, bundle id `com.gameberry.ludomissions`.

Code style (copied from chaupar): clang-format Google base, **tabs width 4**, ColumnLimit 150, left-aligned pointers (`int* p`), braces always. Copy `chaupar/.clang-format` to the repo root.

---

## 4. Board encoding and layout (data; `Models/BoardLayout.{h,cpp}`)

Colours: `RED=0` (bottom-left yard), `GREEN=1` (top-left), `YELLOW=2` (top-right), `BLUE=3` (bottom-right). Flat colours come from chaupar `ViewUtils::getFlatColorByIndex`: RED (234,73,55), GREEN (34,202,87), YELLOW (240,198,31), BLUE (30,144,255).

Constants:
- `TRACK_LEN = 52`, `LAST_TRACK_PROGRESS = 50`, `HOME_LANE_FIRST = 51`, `FINISHED = 56`, `IN_YARD = -1`
- `startCell(c) = c * 13` → 0, 13, 26, 39
- `globalCell(c, progress) = (startCell(c) + progress) % 52`, valid only for progress `0..50`
- Safe global cells: `{0, 8, 13, 21, 26, 34, 39, 47}`. These are the 4 starts plus the 4 stars (start+8). Taken from chaupar `LudoBoardHelper::SAFE_POSITIONS` minus 1.

**Grid:** a 15×15 grid of (col,row) with **row 0 at the bottom** and cell centres at integer coordinates. The table below was verified against `chaupar/LudoProject/assets/StudioProject/ludo_board/ludo_board.prefab` (chaupar `box(i+1)` = our global `i`).

```
TRACK_GRID[52] = {
 {6,1},{6,2},{6,3},{6,4},{6,5},{5,6},{4,6},{3,6},{2,6},{1,6},{0,6},{0,7},{0,8},   // 0..12  (0 RED start, 8 star)
 {1,8},{2,8},{3,8},{4,8},{5,8},{6,9},{6,10},{6,11},{6,12},{6,13},{6,14},{7,14},{8,14}, // 13..25 (13 GREEN start, 21 star)
 {8,13},{8,12},{8,11},{8,10},{8,9},{9,8},{10,8},{11,8},{12,8},{13,8},{14,8},{14,7},{14,6}, // 26..38 (26 YELLOW start, 34 star)
 {13,6},{12,6},{11,6},{10,6},{9,6},{8,5},{8,4},{8,3},{8,2},{8,1},{8,0},{7,0},{6,0}   // 39..51 (39 BLUE start, 47 star)
};
HOME_LANE_GRID[4][5] = {
 {{7,1},{7,2},{7,3},{7,4},{7,5}},        // RED   (entered from global 50 = (7,0))
 {{1,7},{2,7},{3,7},{4,7},{5,7}},        // GREEN (from global 11 = (0,7))
 {{7,13},{7,12},{7,11},{7,10},{7,9}},    // YELLOW(from global 24 = (7,14))
 {{13,7},{12,7},{11,7},{10,7},{9,7}}     // BLUE  (from global 37 = (14,7))
};
YARD_CENTER[4] = {{2.5,2.5},{2.5,11.5},{11.5,11.5},{11.5,2.5}};   // yard spots = centre ± (0.8, 0.8)
FINISH_SPOT[4] = {{7,6.4},{6.4,7},{7,7.6},{7.6,7}};              // centre triangle on the colour's side
```

Self-check that must be encoded as unit tests:
- For every colour c, `TRACK_GRID[globalCell(c,50)]` is orthogonally adjacent to `HOME_LANE_GRID[c][0]` (Manhattan distance 1).
- Every adjacent pair of track entries is 1 grid step apart (Chebyshev distance 1). The inner-corner steps (e.g. 4→5, 17→18) are diagonal, as in standard Ludo.

`BoardLayout` exposes pure functions returning grid coordinates as `GridPos{float col, row}`:
- `trackGrid(globalCell)`
- `homeLaneGrid(color, idx)`
- `yardSpotGrid(color, tokenIdx)`
- `finishGrid(color)`
- `gridForToken(color, tokenIdx, progress)`

Views convert grid to pixels (§9).

---

## 5. Rules and TurnMachine spec (`Controllers/Logic/`)

### 5.1 Models (`Models/MatchState.h`, `MoveOption.h`, `Types.h`)

```cpp
enum class PlayerKind { Human, Bot };
enum class Phase { NotStarted, AwaitingRoll, AwaitingMove, MatchOver };
struct PlayerState { int color; PlayerKind kind; std::array<int,4> progress{-1,-1,-1,-1}; int finishRank = 0; bool sitsOut = false; };
// NO std::string in MatchState: it is copied thousands of times per offer by the Mission Director. Names live in GameConfig / MatchSnapshot.
// sitsOut: endTurn() skips this player exactly like a finished one. Used ONLY by the A* search to freeze bots. Never set in real matches.
struct MatchState {
	std::vector<PlayerState> players;   // index == color (always 4 entries in MVP)
	int selfPlayer = 0;                 // the human
	int current = 0;                    // whose turn
	Phase phase = Phase::NotStarted;
	std::vector<int> pendingRolls;      // STACK_AND_MOVE stack, in roll order
	int consecutiveSixes = 0;
	bool bonusRollPending = false;      // capture/finish bonus not yet rolled
	int turnNumber = 0;                 // increments on every TURN_STARTED (all players)
	std::vector<int> ranking;           // filled at MatchOver
};
struct MoveOption { int player; int token; int value; int from; int to; bool captures; };
```

### 5.2 `Rules` (pure, stateless functions)
- `int targetProgress(int from, int value)`
  - From the yard: returns 0 if `value == 6`, otherwise `INVALID`.
  - From finished: `INVALID`.
  - Otherwise `from + value`, or `INVALID` if the result is above 56.
- `bool isSafeCell(int globalCell)`
- `std::vector<int> capturableTokensAt(const MatchState&, int moverPlayer, int toProgress)`
  - A capture happens only if `toProgress ∈ 0..50`, the cell is not safe, and there is **exactly one opponent token in total** on that global cell. This is chaupar `getKilledPieces`: two or more opponent tokens protect each other.
  - Returns `{player, token}` pairs; there is at most one.
- `std::vector<MoveOption> legalMoves(const MatchState&, int player)`: for each **unique** value in `pendingRolls` × each of the 4 tokens, add an option if `targetProgress` is valid. Order is by token, then value, ascending.
- `std::optional<MoveOption> autoMove(const MatchState&)`: returns an option only when exactly one **distinct token** is movable **and** it has exactly one legal value. This is chaupar `tryMove`/`trySelect`.

### 5.3 `TurnMachine` (pure; mutates a `MatchState&`; returns ordered `std::vector<GameEvent>`)

```cpp
class TurnMachine {
public:
	TurnMachine(MatchState& state, const RulesConfig& rules);
	std::vector<GameEvent> startMatch();                 // MATCH_STARTED, TURN_STARTED(0)
	std::vector<GameEvent> roll(int value);              // precondition phase==AwaitingRoll, 1<=value<=6
	std::vector<GameEvent> move(int token, int value);   // precondition phase==AwaitingMove && legal
	// preconditions violated -> return {} and LM_LOG_ERROR (never assert-crash in release)
};
```

`startMatch()` sets `current = 0` and `turnNumber = 1`, emits `MATCH_STARTED` then `TURN_STARTED{player:0}`, and sets `phase = AwaitingRoll`. The initial token progress is left as it is in the MatchState passed in. Normally that means all −1, but DEV `debug.startProgress` can preset a scenario (§8).

`roll(value)`:
1. `bonusRollPending = false`. Push `value` onto `pendingRolls`. Emit `DICE_ROLLED{player, value}`.
2. If `value == 6`:
   - `consecutiveSixes++`.
   - If `rules.threeSixesForfeit && consecutiveSixes == 3`: pop the last 3 entries of `pendingRolls`, emit `THREE_SIXES{player}`, set `consecutiveSixes = 0`. If `pendingRolls` is now empty, do **endTurn()** and return. Otherwise go to step 4.
   - Otherwise: `phase = AwaitingRoll` and return (the player rolls again before moving).
3. Otherwise `consecutiveSixes = 0`.
4. Resolve:
   - If `legalMoves` is empty: emit `NO_MOVES{player}`, clear `pendingRolls`, **endTurn()**.
   - Otherwise `phase = AwaitingMove`.

`move(token, value)`:
1. Remove **one** instance of `value` from `pendingRolls`. Set `from` and `to`, then set `progress = to`.
2. Emit `TOKEN_MOVED{player, token, from, to, steps, value}`, where `steps = (from == -1) ? 1 : to - from`.
3. If `from == -1`, emit `TOKEN_UNLOCKED`. If `from <= 50 && 51 <= to && to <= 55`, emit `TOKEN_ENTERED_HOME_LANE`. A direct jump from the track to 56 emits only TOKEN_FINISHED.
4. Capture: for the victim, set `progress = -1` and emit `TOKEN_CAPTURED{player, token, victimPlayer, victimToken, cell}`. If `rules.captureBonusRoll`, set `bonusRollPending = true`.
5. If `to == 56`, emit `TOKEN_FINISHED{player, token}`. If `rules.finishBonusRoll`, set `bonusRollPending = true`.
   - If all 4 tokens are at 56: set `finishRank = 1`, emit `PLAYER_FINISHED{player, rank:1}`, then **endMatch()** and return.
6. If `bonusRollPending`: emit `BONUS_ROLL_GRANTED{player, reason: CAPTURE|FINISH}`, set `phase = AwaitingRoll`, and **keep** `pendingRolls`. This is chaupar `EVENT_EXTRA_DICE_ROLL`: roll first, then spend everything.
7. Otherwise, if `pendingRolls` is not empty:
   - If `legalMoves` is not empty: `phase = AwaitingMove`.
   - Otherwise: emit `NO_MOVES`, clear `pendingRolls`, **endTurn()**.
8. Otherwise **endTurn()**.

`endTurn()`:
1. Emit `TURN_ENDED{player}`.
2. Set `current` to the next index whose player is not finished and does not have `sitsOut` set. That can be the same player again, e.g. in the A* search where all bots sit out.
3. Reset `pendingRolls`, `consecutiveSixes` and `bonusRollPending`.
4. `turnNumber++`.
5. Emit `TURN_STARTED{player: current}`, then set `phase = AwaitingRoll`.

`endMatch()`:
1. Rank the others by total progress, highest first; ties go to the lower index. Fill `ranking` and `finishRank`.
2. Set `phase = MatchOver`.
3. Emit `TURN_ENDED{player}`, then `MATCH_ENDED{player: winner}`.

TURN_ENDED is always emitted, so missions can close their windows.

### 5.4 `BoardQueries` (pure; shared by BotBrain and mission conditions)
- `int enemyAheadDistance(state, player, token, int maxD)`
  - Returns the smallest `d ∈ 1..maxD` such that the token (progress `0..50`, and `progress + d <= 50`) would land on a non-safe global cell holding **exactly one** enemy token.
  - Returns 0 if there is none.
- `int enemyBehindDistance(state, player, token, int maxD)`
  - Returns the smallest `d ∈ 1..maxD` such that an enemy token E, with progress `pe ∈ 0..50` and `pe + d <= 50`, sits on `globalCell(own) - d (mod 52)`, and the own token's cell is not safe.
  - Returns 0 if there is none.
- `int countTokens(state, player, Zone zone)`, where `Zone` is one of `Yard | Track | HomeLane | Finished | OutOfYard` (OutOfYard = progress ≥ 0, including finished).
- `bool anyEnemyAhead(state, player, min, max)`, `bool anyEnemyBehind(state, player, min, max)`, `int maxProgress(state, player, excludeFinished=true)`.

### 5.5 `BotBrain` (pure)
`MoveOption choose(const MatchState&, const std::vector<MoveOption>&, Rng&)`. Each option gets a score and the highest wins; ties are broken by `rng`.

| Condition | Points |
|---|---|
| Captures | +100 |
| Finishes (to==56) | +80 |
| Unlock (from==-1) | +60 |
| Enters home lane | +50 |
| Lands on a safe cell | +30 |
| Escapes (the token is threatened now: `enemyBehindDistance > 0`, and the destination is safe or not threatened) | +40 |
| The destination is threatened (simulate the move on a copy of the state, then check `enemyBehindDistance`) | −50 |
| Progress | `+ to * 0.5` |

### 5.6 `Rng`
A wrapper over `std::mt19937` with `seed(uint32_t)`, `int dice()` (1..6) and `int range(lo,hi)`. Seed 0 means use `std::random_device`.

---

## 6. Events

### 6.1 EventBus (`Events/EventBus.{h,cpp}`): a typed wrapper over axmol's EventDispatcher

Chaupar's `commons::EMEventListener` uses `ax::Value` payloads and makes you register by hand. We follow chaupar's `TypedListenerUtils` idea instead: each event is a struct with a compile-time `NAME`, so a name can never be paired with the wrong payload type.

```cpp
namespace lm {
class EventBus {
public:
	template <class T> static void publish(const T& e) {
		ax::Director::getInstance()->getEventDispatcher()->dispatchCustomEvent(T::NAME, const_cast<T*>(&e));
	}
	template <class T> static void subscribe(void* owner, std::function<void(const T&)> fn) {
		auto* l = ax::EventListenerCustom::create(T::NAME, [fn](ax::EventCustom* ev) { fn(*static_cast<const T*>(ev->getUserData())); });
		ax::Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(l, 1);  // NEVER priority 0 (asserts)
		s_listeners[owner].push_back(l);
	}
	static void unsubscribeAll(void* owner);   // removeEventListener for each, erase owner
private:
	static std::unordered_map<void*, std::vector<ax::EventListener*>> s_listeners;
};
}
```

Usage: the template argument is **required**, because a lambda can't be deduced to `std::function<void(const T&)>`.
```cpp
EventBus::subscribe<UiPlayTapped>(this, [this](const UiPlayTapped&) { onPlay(); });
EventBus::publish(UiPlayTapped{});
```

Rules:
- Dispatch is **synchronous**. A payload pointer is valid **only during the callback**, so copy it if you need it later.
- Never publish from a constructor or `init()`.
- Every subscriber must call `unsubscribeAll(this)` (in Views, `BaseView::onExit`; in Controllers, `reset()`).

### 6.2 Event catalogue (event names use the `"lm.<area>.<name>"` format)

**UI → Controllers** (`UiEvents.h`)
- `UiLobbyReady{}`
- `UiPlayTapped{}`
- `UiGameSceneReady{}`
- `UiGameSceneExiting{}`, published from `GameScene::beforeExit()`. GameController aborts the match and MissionController voids its missions.
- `UiRollDiceTapped{}`
- `UiTokenTapped{int player; int token;}`
- `UiRollChosen{int player; int token; int value;}`
- `UiResultClosed{bool playAgain;}`

**Debug (DEV only, View → Controllers)** (`DebugEvents.h`)
- `DebugForceNextRoll{int value;}` applies to the **human's** next roll only; bots keep rolling randomly.
- `DebugReloadConfig{}`
- `DebugToggleFastBots{}`
- `DebugResetCoins{}`
- `DebugCycleForcedMission{}`: pressing M cycles `forcedMission` through "" and each loaded mission id. When it is set, the offer phase offers **that** mission if its offerWhen is true, instead of picking by weight.

**Game → Views/Missions** (`GameEvents.h`)
- `GameEventMsg{GameEvent event; float animScale;}` is the single channel for all domain events in §6.3.
  - `animScale` is 1.0, or `fastBotsMultiplier` when the event's player is a bot and fast bots is on.
  - Views multiply **every** animation duration for that event by `animScale`, so the view animations always match the controller's pacing.
- `MatchSnapshot{MatchState state; TimingConfig timing; std::vector<std::string> names;}` is published right after the MATCH_STARTED batch is queued, for a full initial render. Views keep `timing` for their animation durations.

**Config/Debug feedback** (`AppEvents.h`)
- `ConfigReloaded{int errors;}` is published by ConfigController.
- `MissionsReloaded{int count; int errors; int warnings;}` is published by MissionController.
- `DebugStateChanged{int forcedRoll; bool fastBots; std::string forcedMission;}` is published by GameController and MissionController for the DEV label.
- `AwaitingRollMsg{int player; bool isHuman;}`
- `AwaitingMoveMsg{int player; bool isHuman; std::vector<MoveOption> options;}`
- `RollChoiceRequested{int player; int token; std::vector<int> values;}`
- `PendingRollsChanged{int player; std::vector<int> rolls;}`

**Missions → Views/Wallet** (`MissionEvents.h`)
- `MissionUpdated{MissionUpdate update;}`
- `MissionMatchSummary{int completed; int failed; int coinsEarned;}`

**Wallet → Views** (`WalletEvents.h`)
- `WalletChanged{int balance; int delta; std::string reason;}`

### 6.3 `GameEvent` (`Models/GameEvent.h`): a flat struct the mission DSL can reflect on

```cpp
enum class GameEventType { MATCH_STARTED, TURN_STARTED, DICE_ROLLED, THREE_SIXES, NO_MOVES, TOKEN_MOVED, TOKEN_UNLOCKED,
	TOKEN_ENTERED_HOME_LANE, TOKEN_CAPTURED, TOKEN_FINISHED, BONUS_ROLL_GRANTED, TURN_ENDED, PLAYER_FINISHED, MATCH_ENDED };
struct GameEvent {
	GameEventType type; int player = -1; int token = -1; int from = -1; int to = -1; int steps = 0; int value = 0;
	int victimPlayer = -1; int victimToken = -1; int cell = -1; int reason = 0; int rank = 0;
	std::optional<int> field(std::string_view name) const;   // "player","token","from","to","steps","value","victimPlayer","victimToken","cell","reason","rank"
};
const char* toString(GameEventType); std::optional<GameEventType> gameEventTypeFromString(std::string_view);
enum BonusReason { BONUS_CAPTURE = 1, BONUS_FINISH = 2 };   // GameEvent.reason for BONUS_ROLL_GRANTED
```

Fields set per event type (every other field keeps its default):

| Event | Fields |
|---|---|
| DICE_ROLLED | player, value |
| TOKEN_MOVED | player, token, from, to, steps, value |
| TOKEN_UNLOCKED / TOKEN_ENTERED_HOME_LANE / TOKEN_FINISHED | player, token, from, to |
| TOKEN_CAPTURED | player (capturer), token, victimPlayer, victimToken, cell |
| BONUS_ROLL_GRANTED | player, reason |
| PLAYER_FINISHED | player, rank |
| MATCH_ENDED | player (winner) |
| all others | player |

---

## 7. Mission system (the designer-facing core)

### 7.1 `missions.json` schema

```json
{
  "version": 1,
  "missions": [
    {
      "id": "capture_in_3",                 // unique, snake_case; required
      "enabled": true,                       // optional, default true
      "title": "Hunter",                     // required
      "description": "Capture an enemy token within {turns} turns",   // placeholders: {target} {turns} {turnsLeft} {progress}
      "reward": { "coins": 50 },            // required, coins >= 0
      "turns": 3,                            // required, >= 1 (window in human turns, includes the offer turn)
      "weight": 10,                          // optional, default 10, > 0
      "cooldownTurns": 4,                    // optional, default = game_config missions.defaultCooldownTurns
      "maxPerMatch": 2,                      // optional, default 0 = unlimited
      "moments": ["turnStart"],              // optional, default ["turnStart"]; allowed: "turnStart", "afterRoll"
      "offerWhen": { "type": "enemyAhead", "min": 1, "max": 6 },  // optional, default {"type":"always"}
      "objective": { "type": "count", "event": "TOKEN_CAPTURED", "where": { "player": "self" }, "target": 1 }
    }
  ]
}
```

- **Comments are NOT allowed** in the real file. rapidjson is strict; the comments above are for this doc only. An optional `"_note"` string field on any object is ignored, for designer notes.
- **Unknown keys produce a warning**, not an error, so a typo like `"trun"` is caught.

### 7.2 Building blocks

**Conditions** (used by `offerWhen` and by the `state` objective). All values are integers.

| type | params | true when |
|---|---|---|
| `always` | – | always |
| `all` | `of: [cond...]` | every child is true |
| `any` | `of: [cond...]` | at least one child is true |
| `not` | `cond: {...}` | the child is false |
| `enemyAhead` | `min=1,max=6` | `BoardQueries::anyEnemyAhead(state, self, min, max)`: **some** d in [min,max] works for some self token (not "the smallest d") |
| `enemyBehind` | `min=1,max=6` | `BoardQueries::anyEnemyBehind(state, self, min, max)` |
| `tokenCount` | `zone: yard/track/homeLane/finished/outOfYard`, `who: self/enemy` (enemy = the sum over all enemies), `op: eq/ne/lt/lte/gt/gte`, `value` | the count compares true |
| `tokenProgressAtLeast` | `value` | some unfinished self token has progress ≥ value |
| `selfTurnNumber` | `op`, `value` | the human's turn counter compares true (1 = first human turn) |
| `canCaptureNow` | – | `state.current == self && phase == AwaitingMove`, and some `Rules::legalMoves` option has `captures == true`. Meant for `afterRoll`. |

**Objectives**

| type | params | behaviour |
|---|---|---|
| `count` | `event`, `where{}`, `target` | +1 for each matching event; completes at `target` |
| `sum` | `event`, `where{}`, `field`, `target` | adds `event.field(field)`; completes at ≥ target |
| `streak` | `event`, `where{}`, `target` | Counts consecutive human turns that contain ≥1 matching event. It updates **only at TURN_ENDED(self)**, in `onSelfTurnEnded`: if the turn matched, `streak++`, otherwise `streak = 0`. It completes when `streak >= target`. It **fails early** when `(target - streak) > turnsLeftAfter`, where `turnsLeftAfter` is the value after the decrement. Progress = streak. |
| `avoid` | `event`, `where{}` | Fails on the first matching event and **completes** when the window closes. progress = turns survived, target = `turns` (so the HUD shows "2/4"). |
| `state` | `cond: {...}` | Completes as soon as `cond` is true after any event. The mission is **not offered** if `cond` is already true at offer time. progress = 0, target = 1 (becomes 1/1 on completion). |

**`where` filter** (EventFilter): an object mapping a field name to a matcher. The event matches when every entry matches.
- Scalar: `"value": 6` is an equality test.
- Operator object: `"steps": {"gte": 5}`. Supported operators: `eq, ne, lt, lte, gt, gte, in:[..]`.
- **Role strings for player fields** (`player`, `victimPlayer`):
  - `"self"` means the value == self.
  - `"enemy"` means the value ≥ 0 and != self.
  - `"any"` means the value ≥ 0.
- `maxPerMatch` counts **offers**.
- An unknown field name is a **load error** for that mission. Valid field names: player, token, from, to, steps, value, victimPlayer, victimToken, cell, reason, rank.

### 7.3 Starter `missions.json` (8 placeholders plus 1 afterRoll demo, final values)

All missions use `moments: ["turnStart"]` except `strike_now`.

| id | title | turns | coins | weight | cooldown | offerWhen | objective |
|---|---|---|---|---|---|---|---|
| `strike_now` (moments: afterRoll) | Strike Now! | 1 | 30 | 10 | 3 | canCaptureNow | count TOKEN_CAPTURED {player:self} target 1 |
| `roll_six_now` | Lucky Six | 1 | 20 | 10 | 3 | always | count DICE_ROLLED {player:self,value:6} target 1 |
| `capture_in_3` | Hunter | 3 | 50 | 12 | 4 | enemyAhead 1–6 | count TOKEN_CAPTURED {player:self} target 1 |
| `finish_in_5` | Homecoming | 5 | 40 | 10 | 5 | tokenProgressAtLeast 44 | count TOKEN_FINISHED {player:self} target 1 |
| `double_capture_6` | Rampage | 6 | 100 | 5 | 8 | tokenCount track enemy gte 2 AND tokenCount outOfYard self gte 1 | count TOKEN_CAPTURED {player:self} target 2 |
| `move_25_in_3` | Sprinter | 3 | 30 | 8 | 4 | tokenCount outOfYard self gte 1 | sum TOKEN_MOVED {player:self} field steps target 25 |
| `survive_4` | Untouchable | 4 | 40 | 8 | 5 | enemyBehind 1–6 | avoid TOKEN_CAPTURED {victimPlayer:self} |
| `six_streak_2` | Hot Hand | 2 | 60 | 6 | 6 | always | streak DICE_ROLLED {player:self,value:6} target 2 |
| `three_out_4` | Full Deploy | 4 | 35 | 8 | 6 | tokenCount outOfYard self lte 2 | state tokenCount outOfYard self gte 3 |

### 7.4 Models (pure data)
- `Params` (`Models/Params.h`): `using ParamValue = std::variant<int64_t, double, bool, std::string>`. It is a small class holding
  - `std::map<std::string, ParamValue>`,
  - `std::map<std::string, Spec>` children,
  - `std::map<std::string, std::vector<Spec>>` lists,
  - getters `getInt(key, def)`, `getString(key, def)`, `has(key)`.
- `Spec { std::string type; Params params; }` is a parsed but uncompiled building block.
- `MissionDef { id, title, description, rewardCoins, turns, weight, cooldownTurns, maxPerMatch, enabled; Spec offerWhen; Spec objective; }`
- `MissionInstance { int uid; std::string id; int progress; int target; int turnsLeft; int rewardCoins; std::string title, description; }`
  - `description` is **re-rendered by the engine on every update** from the def's template, with `{target} {turns} {turnsLeft} {progress}`. The view just displays it.
  - `uid` increments per offer, so the same id offered twice in a match is still distinguishable in views.
- `MissionUpdate { enum Kind {Offered, Progress, Completed, Failed, Voided} kind; MissionInstance instance; }`

### 7.5 Logic (pure)
- `struct EvalContext { const MatchState& state; int self; int selfTurnIndex; };` is passed to everything, so that `selfTurnNumber` can be implemented.
- `Condition` interface: `virtual bool eval(const EvalContext&) const = 0;`
- `Objective` interface. These are **runtime objects per instance**, with state inside. Each objective is created fresh on every offer from a factory.
  ```cpp
  struct ObjectiveResult { bool changed=false; bool completed=false; bool failed=false; };
  class Objective { public:
    virtual ~Objective() = default;
    virtual void begin(const EvalContext&, int turns) {}
    virtual ObjectiveResult onEvent(const GameEvent&, const EvalContext&) = 0;        // called for every event (incl. TURN_ENDED)
    virtual ObjectiveResult onSelfTurnEnded(int turnsLeftAfter) { return {}; }       // after decrement; streak bookkeeping / fail-fast; avoid: progress++
    virtual bool completesOnWindowEnd() const { return false; }                      // avoid => true
    virtual bool alreadySatisfied(const EvalContext&) const { return false; }        // state => cond.eval
    virtual int progress() const = 0; virtual int target() const = 0;
    // --- needed by the Mission Director (§7.7) ---
    virtual std::unique_ptr<Objective> clone() const = 0;                           // deep copy incl. runtime state
    virtual uint64_t stateKey() const { return (uint64_t)progress(); }               // for A* visited-set dedup (streak: streak*2+metThisTurn)
    virtual int minTurnsHint(const EvalContext&, int turnsLeft) const { return 0; }  // ADMISSIBLE lower bound on remaining human turn-ends; 0 is always safe
  };
  ```
- `MissionTracker` (pure; `Missions/MissionTracker.{h,cpp}`) is **the single implementation of the window and resolution rules**. It is shared by MissionEngine, the A\* search and the Monte Carlo rollouts, so all three always agree.
  ```cpp
  enum class TrackStatus { Active, Completed, Failed };
  class MissionTracker { public:
    MissionTracker(std::unique_ptr<Objective> obj, int turns);
    MissionTracker(const MissionTracker&);                 // deep copy via obj->clone()
    // Runs §7.5 steps 1–2 for ONE record: objective.onEvent, then (on TURN_ENDED(self)) turnsLeft--, onSelfTurnEnded, window end.
    TrackStatus feed(const GameEvent&, const EvalContext&, bool* progressed = nullptr);
    int turnsLeft() const; const Objective& objective() const; };
  ```
  MissionEngine's active records each hold a `MissionTracker`, and §7.5 steps 1–2 become `status = tracker.feed(e, ctx, &changed)`.
- `ConditionRegistry` / `ObjectiveRegistry`: `std::unordered_map<std::string, Entry>`, where `Entry{ std::vector<std::string> required; std::vector<std::string> optional; Factory make; }`.
  - Factories take a `const Spec&` plus the other registry (for nested conditions) and return `std::unique_ptr<...>`, or an error string.
  - Built-ins are registered in `registerBuiltinConditions(ConditionRegistry&)` in `BuiltinConditions.cpp` (same for objectives).
  - **Do not rely on static-initialiser self-registration.** It breaks with static libs and init order. Call the register functions explicitly from `MissionEngine`'s constructor.
- `MissionParser::parse(const std::string& json, const ConditionRegistry&, const ObjectiveRegistry&) -> ParseResult { std::vector<MissionDef> defs; std::vector<std::string> errors; std::vector<std::string> warnings; }`
  - A JSON syntax error is converted from the rapidjson offset to a **line:col** message.
  - Each mission is validated on its own. A bad mission is skipped with the message `missions.json: mission 'capture_in_3': objective.where: unknown field 'playr'`, and the other missions still load.
  - Validation includes a trial compile of `offerWhen` and `objective` through the registries.
  - A duplicate id is an error for the second copy.
- `TextTemplate::render(std::string tpl, const std::map<std::string,int>& vars)` replaces `{name}`. An unknown placeholder is left as-is.
- `MissionEngine`
  ```cpp
  class MissionEngine { public:
    MissionEngine();                                       // registers builtins
    ParseResult loadFromJson(const std::string& json);     // replaces defs for FUTURE offers; active instances keep their own compiled objects
    void setSettings(const MissionSettings&);              // maxActive, offersPerTurn, defaultCooldownTurns
    void startMatch(int selfPlayer, uint32_t seed);        // clears actives, cooldowns, counters, selfTurnIndex=0
    void setForcedMission(std::string id);                 // DEV: "" = off
    std::vector<std::string> definitionIds() const;        // for the M-key cycle
    std::vector<MissionUpdate> onEvent(const GameEvent&, const MatchState&);
    std::vector<MissionUpdate> endMatch();                 // Voided for each active
    const std::vector<MissionInstance> activeInstances() const; };
  ```
  The engine keeps definitions as `std::shared_ptr<const CompiledMission>` (the def plus the compiled offer condition plus an objective factory). Each active record holds its own shared_ptr, so a hot reload can never leave a dangling pointer.

**`onEvent` algorithm (exact order).** The engine keeps `int selfTurnIndex = 0`. It is incremented at the very start of handling TURN_STARTED(self), so the first human turn is 1.
0. If `e.type == TURN_STARTED && e.player == self`: `selfTurnIndex++`.
1. For each active record, `r = objective.onEvent(e, ctx)`:
   - If `r.completed`: mark it Completed.
   - Else if `r.failed`: mark it Failed.
   - Else if `r.changed`: emit Progress.
2. If `e.type == TURN_ENDED && e.player == self`: for each **unresolved** active record:
   - `turnsLeft--`, then `r = objective.onSelfTurnEnded(turnsLeft)`.
   - If `r.failed`: mark Failed.
   - Else if `r.completed`: mark Completed.
   - Else if `turnsLeft == 0`: mark Completed if `completesOnWindowEnd()`, otherwise Failed.
   - Else emit Progress (so the view refreshes turnsLeft).
3. **Resolve marked records.** Build a new vector: **never erase inside a range-for.** For each marked record:
   - Emit Completed or Failed.
   - Set `cooldownUntil[id] = selfTurnIndex + cooldownTurns + 1`. A mission resolved in human turn j can be offered again at turn j + cooldownTurns + 1, whether it resolved mid-turn or at the turn's end.
4. **Offer phase.** Work out the moment:
   - `TurnStart` if `e.type == TURN_STARTED && e.player == self`. Also reset `offersThisTurn = 0` and `offersThisMoment[*] = 0`.
   - `AfterRoll` if `e.type == DICE_ROLLED && e.player == self && state.current == self && state.phase == AwaitingMove`.
     - Why the extra checks: when the roll led to NO_MOVES, the state has already moved on to the next player by the time DICE_ROLLED is dispatched, so the check fails correctly.
     - After a 6 the phase is AwaitingRoll, so no offer is made until the non-6 roll.
   - Otherwise there is no offer phase.

   Then, while `active < maxActive && offersThisTurn < offersPerTurn && offersThisMoment[moment] < offersPerMoment[moment]`:
   - **Candidates** are every def that:
     - is enabled
     - has `moment ∈ def.moments`
     - is not active
     - has `selfTurnIndex >= cooldownUntil[id]` (default 0)
     - has `offersThisMatch[id] < maxPerMatch` (or maxPerMatch == 0)
     - has `offerWhen.eval(ctx) == true`
     - and whose freshly made objective is not `alreadySatisfied(ctx)`.
   - If `forcedMission` (DEV) is a candidate, pick it.
   - Otherwise `choice = m_strategy->choose(candidates, ctx, moment, stats)`. The `IOfferStrategy` is either `WeightedRandomStrategy`, or `DirectorStrategy` (§7.7) when `director.enabled`. The strategy may return **nothing**, meaning no offer at this moment; in that case break.
   - Create the instance (turnsLeft = turns, progress 0, rendered text), call `begin`, increment `offersThisMatch[id]`, `offersThisTurn` and `offersThisMoment[moment]`, then emit Offered.
   - A `state` mission can't complete on the event that offered it, because `alreadySatisfied` already excluded it.
   - An afterRoll offer's window includes the current turn, so `turns: 1` means "during this turn".
5. If `e.type == MATCH_ENDED`: emit Voided for all actives, then clear them.

### 7.6 Checking coverage of the 8 starters (write these as engine unit tests driven by scripted `GameEvent`s and hand-built `MatchState`s)
- `roll_six_now`: offered on TURN_STARTED(self). DICE_ROLLED(self, 6) completes it. Otherwise TURN_ENDED(self) fails it.
- `six_streak_2`:
  - A 6 in turn 1 then a 6 in turn 2 completes it at TURN_ENDED of turn 2.
  - No 6 in turn 1 fails it at the end of turn 1: (2 − 0) > 1.
  - A 6 in turn 1 does **not** fail at the end of turn 1: (2 − 1) > 1 is false.
- `survive_4`: a TOKEN_CAPTURED with victimPlayer=self on a bot turn fails it. After 4 human turn ends with no capture, it completes.
- `three_out_4`: not offered when 3 tokens are already out. Completes right after the TOKEN_MOVED that unlocks the 3rd token.
- `move_25_in_3`: sums steps. An unlock counts 1 step.
- Plus:
  - maxActive = 3 is respected.
  - offersPerTurn = 1.
  - No duplicate active ids.
  - Cooldown blocks a re-offer.
  - MATCH_ENDED voids all.
  - Hot reload keeps active instances alive.
  - `strike_now` is offered only at afterRoll, when the pending roll can capture. It is never offered at turnStart.

### 7.7 Mission Director: the serving algorithm (`Controllers/Logic/Missions/Director/`, pure)

**Goal:** serve the mission that is *possible*, *about the right difficulty for this player*, and *relevant right now*, or serve nothing. The work is done by these files:

```
Director/ IOfferStrategy.h  WeightedRandomStrategy.{h,cpp}  DirectorStrategy.{h,cpp}
          FeasibilitySearch.{h,cpp}   (stage 1: A*)
          RolloutSimulator.{h,cpp}    (stage 2: Monte Carlo)
          UtilityScorer.{h,cpp}       (stage 3)
          DifficultyTracker.{h,cpp}   (adaptive band)
```

```cpp
struct Candidate { std::shared_ptr<const CompiledMission> mission; };
struct OfferStats { std::map<std::string,int> offersThisMatch; std::string lastOfferedId; int humanRaceRank; };   // rank 1..4 by total progress
class IOfferStrategy { public:
  virtual ~IOfferStrategy() = default;
  virtual std::optional<size_t> choose(const std::vector<Candidate>&, const EvalContext&, OfferMoment, const OfferStats&, Rng&) = 0;
  virtual void onResolved(const std::string& id, bool completed) {}   // DirectorStrategy forwards to DifficultyTracker
};
```

**Stage 1: `FeasibilitySearch` (A\*)**
- **Question it answers:** "In the best case, how many human turn-ends are needed to complete this mission?" It returns `SearchResult{ enum {Feasible, Infeasible, Unknown} verdict; int minTurns; int expansions; }`.
- **Relaxation (this is what makes it a lower bound):**
  - Dice are *chosen*, not random: every roll node branches over 1..6.
  - Bots are frozen: in a copy of the MatchState, every non-self player gets `sitsOut = true`, so `endTurn()` hands the turn straight back to self.
  - TurnMachine rules still apply unchanged: three sixes forfeit, exact finish, captures, bonus rolls. This is what correctly makes "a 20-cell capture in 1 turn" infeasible (max 6+6+5 = 17).
- **Node:** `{ MatchState s; MissionTracker t; int g; }`, where g = the number of human TURN_ENDED events so far.
- **Successors:** use a `TurnMachine` on a copy.
  - If `s.phase == AwaitingRoll`, create one child per `v ∈ 1..6` via `roll(v)`.
  - If `s.phase == AwaitingMove`, create one child per `Rules::legalMoves` option via `move(token, value)`.
  - Feed every emitted event into the child's tracker. Edge cost = the number of `TURN_ENDED(self)` events in the batch. That is 0 for a 6 or a bonus roll, 1 for a move that ends the turn. So this is a 0/1-cost graph.
- **Goal:** `tracker` returns Completed. For `avoid`, that means surviving to the end of the window, which is trivially feasible because the bots are frozen. **Pruned:** Failed, MatchOver without completion, or `g > turns`.
- **Priority:** `f = g + h`, with `h = tracker.objective().minTurnsHint(ctx, turnsLeft)`.
  - Built-in hints: every objective returns 0 (always admissible) except `streak`, which returns `max(0, target − streak − (metThisTurn ? 1 : 0))`.
  - Use a min-heap on `(f, g)`, tie-broken by insertion order for determinism.
- **Visited set:** a key hashed from all 16 progress values, the sorted `pendingRolls`, `consecutiveSixes`, `bonusRollPending`, `phase`, and `tracker.objective().stateKey()` plus `turnsLeft`. Skip a node if it was already seen with a lower or equal g.
- **Budget:** `director.astarMaxExpansions` (default 4000). If the budget runs out, return **Unknown** (treated as feasible, so a mission is never wrongly dropped). `minTurns` is then the lowest f still in the heap.
- **Output use:** Infeasible means the candidate is dropped. `minTurns == 0` means "can be completed in this very turn", which feeds the timeliness term.
- It is honest about what it is: a lower bound with frozen bots. Bot moves can only make captures easier or harder in ways it ignores. That is why it is only a **filter**, and stage 2 measures the real probability.

**Stage 2: `RolloutSimulator` (Monte Carlo)**
- For each surviving candidate, run up to `director.rollouts` (default 96) simulations. Each one:
  1. Copies the real MatchState (bots **not** frozen), makes a fresh `MissionTracker` from the mission factory, calls `begin`, and creates a `TurnMachine` on the copy.
  2. Loops until the tracker resolves, MatchOver, or 400 commands (a safety cap that counts as Failed):
     - AwaitingRoll → `roll(rng.dice())`.
     - AwaitingMove as a bot → `BotBrain::choose`.
     - AwaitingMove as self → the **mission-seeking policy**: for each legal option, simulate it on a copy, feed the events into a *cloned* tracker, and score it `1000·completed − 1000·failed + 100·(progressΔ) + BotBrain score`. Take the argmax, tie-broken by rng.
     - Feed every emitted event into the tracker.
- Output: `P = completions / rollouts`, plus `meanTurns` over the successes.
- **Determinism:** a single `Rng` seeded from the engine's rng. Candidates are processed in JSON order, and rollouts are interleaved round-robin across candidates.
- **Time budget:** `director.simBudgetMs` (default 12). Stop early when the budget is used; each candidate's P uses however many rollouts it got, with a minimum of 16. If a candidate got fewer than 16, drop it for this moment.
- `simBudgetMs = 0` means *no time limit*, so the exact rollout count is used. **Tests must use 0**, because time budgets make results machine-dependent.

**Stage 3: `UtilityScorer` plus selection**
- `center` comes from the DifficultyTracker. If `stats.humanRaceRank == 4` (last in the race), use `center + behindBias`, clamped. Here center is the **target success probability**: higher means easier.
- `fit = exp(-((P − center) / halfWidth)²)`. It peaks at the target and never reaches 0, so there is always a best option.
- `timely = 1 + timelyBonus · (minTurns == 0 ? 1 : 0)`
- `novelty = 1 / (1 + offersThisMatch[id])^noveltyPower × (id == lastOfferedId ? repeatPenalty : 1)`
- `U = def.weight/10 × fit × timely × novelty`
- **Gate:** if `max U < minUtility`, return nothing. No offer is made at this moment, which is the "right moment" rule.
- **Selection:** softmax sampling, `p_i ∝ U_i^(1/temperature)` (temperature → 0 means argmax), using the passed `Rng`.
- **DEV explainability log**, one line per decision, sorted by U: `[LM][Director] afterRoll center=0.50 | strike_now P=0.61 minT=0 U=0.93 PICK | capture_in_3 P=0.34 minT=1 U=0.41 | finish_in_5 INFEASIBLE`. This is how designers tune.

**`DifficultyTracker`**
- `center` starts at `startCenter` (0.5).
- `onResolved(completed)`: add `stepOnComplete` (−0.05, harder) on completion, or `stepOnFail` (+0.05, easier) on failure, then clamp to [min, max] (0.2, 0.85).
- Pure. MissionController persists `center` in `UserDefault` under key `lm.director.center` when `persist` is true, and restores it at init.
- `MissionEngine` calls `m_strategy->onResolved(id, completed)` in §7.5 step 3. Voided missions don't count.

**Performance guardrails**
- Everything is copy-based, so MatchState must stay small: no strings, and no heap allocations beyond the two small vectors.
- Worst case at one moment: 9 candidates × 96 rollouts × about 60 commands. That is time-boxed by `simBudgetMs`.
- The work runs synchronously inside the `GameEventMsg` handler, while the controller is anyway waiting for the next paced event, so a hitch of 12 ms or less is invisible.
- Log the elapsed ms per decision in DEV.
- **Optional (only if needed):** move the evaluation to a worker thread. It is not planned for the MVP.

---

## 8. `game_config.json`

```json
{
  "rules":   { "threeSixesForfeit": true, "captureBonusRoll": true, "finishBonusRoll": true },
  "players": { "humanColor": 0, "names": ["You", "Bot Green", "Bot Yellow", "Bot Blue"] },
  "timing":  { "diceRollAnim": 0.6, "tokenStep": 0.18, "captureAnim": 0.45, "botThinkDelay": 0.6,
               "autoMoveDelay": 0.35, "turnGap": 0.35, "resultPopupDelay": 1.5, "fastBotsMultiplier": 0.25 },
  "missions":{ "enabled": true, "file": "config/missions.json", "maxActive": 3, "offersPerTurn": 2,
               "offersPerMoment": { "turnStart": 1, "afterRoll": 1 }, "defaultCooldownTurns": 3 },
  "director":{ "enabled": true, "astarMaxExpansions": 4000, "rollouts": 96, "simBudgetMs": 12,
               "difficulty": { "startCenter": 0.5, "halfWidth": 0.15, "stepOnComplete": -0.05, "stepOnFail": 0.05,
                               "min": 0.2, "max": 0.85, "behindBias": 0.1, "persist": true },
               "utility":    { "timelyBonus": 0.5, "noveltyPower": 1.0, "repeatPenalty": 0.5, "temperature": 0.5, "minUtility": 0.05 } },
  "debug":   { "rngSeed": 0, "fastBots": false, "forcedMission": "",
               "startProgress": [] }
}
```

- `debug.startProgress` (DEV scenario setup) is either empty, or a 4×4 array of progress values that GameController copies into the MatchState before `startMatch()`.
  - Example Hunter demo: `[[14,-1,-1,-1],[4,-1,-1,-1],[-1,-1,-1,-1],[-1,-1,-1,-1]]`.
    - RED token 0 is at progress 14, which is global 14.
    - GREEN token 0 is at progress 4, which is global (13+4) = 17. It is exactly 3 ahead of RED, and 17 is not a safe cell.
    - So `capture_in_3` is eligible on turn 1. Press M to force it, press 3, then roll: that's a capture.
    - Put this exact case in a unit test too.
  - It is ignored when `LM_DEV` is off.
- `debug.forcedMission` sets the initial value for the M-key cycle (§6.2).

- The MVP keeps `humanColor` fixed at 0. The parser may reject other values with a warning.
- `ConfigParser::parseGameConfig(json) -> {GameConfig, errors}` is pure. Missing keys fall back to the defaults above, so a partial file still works.
- `GameConfig` lives in `Models/GameConfig.h` as structs `RulesConfig`, `TimingConfig`, `MissionSettings`, `DebugConfig`.

---

## 9. Views and assets

### 9.1 Layout (`Views/Common/UiConfig.h`: all constants in one place, design space 720x1280, origin bottom-left)

With FIXED_WIDTH, the visible height changes with the device aspect ratio: 1280 on 9:16, about 1560 on tall iPhones.
- Anchor the **top bar and mission HUD to the top**: `y = visibleOrigin.y + visibleSize.height - offset`, clamped inside `Director::getSafeAreaRect()`.
- Anchor the **bottom panels to the bottom** (same approach).
- Keep the board centred vertically. On the Mac window (9:16) the numbers below are exact.

- Top bar: y=1235. Coin counter at the top-right (x=610); title / back button at the top-left.
- Mission HUD: 3 cards, each 224x120, centred at y=1110. Card x-centres: 124, 360, 596.
- Player panels: 4 corners, each 330x80.
  - Top-left and top-right panels centred at y=1005 (bottom edge at 965, clear of the board's top at 955).
  - Bottom-left and bottom-right panels centred at y=215 (top edge at 255, clear of the board's bottom at 265).
  - Each panel shows the name, a colour dot, the dice slot (72x72) and a pending-rolls chip row.
- Board: 690x690, centred at (360, 610). `CELL = 46`.
- Toasts: centre at y=760, slide in from the top of the board.
- **BoardGeometry**: `Vec2 gridToLocal(GridPos g) { return Vec2((g.col - 7) * CELL, (g.row - 7) * CELL); }`, in BoardView-local coordinates with BoardView anchored at its centre.

### 9.2 Board rendering (BoardView, all DrawNode + a few sprites; **this fork's DrawNode takes `Color4B`, not Color4F**)
1. White rounded background: `flat_round_white.png` as a Scale9Sprite, 700x700, grey tint.
2. Yards: a 6x6-cell square in the player colour, a 4x4-cell white inner square, and 4 circles (radius 0.38 CELL) in the colour at the yard spots.
3. Track cells: white squares of 0.94 CELL with a 1px grey border. Start cells are filled with the owner's colour. Star cells get `star_outlined.png` (80px → scale 0.5).
4. Home lanes: cells filled with the colour.
5. Centre: 4 triangles via `drawSolidPoly`, from centre (7,7) to the corners of the 3x3 middle block, each in its colour on its own side (RED at the bottom, GREEN on the left, YELLOW at the top, BLUE on the right).

### 9.3 Tokens, dice, highlights
- **TokenView**: `token_ludo_basic_base.png` (85x90) plus `token_ludo_basic_color.png` (71x71) tinted with the player colour, at scale 0.5 and an offset of +4px y (tune by eye).
  - Highlight: `token_basic_shine.png` rotating 360°/s plus a scale pulse. This is chaupar `PieceHighlighter`.
  - Several tokens on one cell: scale 0.75 with offsets `{(-8,-8),(8,-8),(-8,8),(8,8)}`.
- **Move animation**: one `MoveTo(tokenStep)` per step through each intermediate grid cell. Yard→start is a single step. Plays `token_move.mp3` on each step.
  - Capture: the victim does a `JumpTo`/`MoveTo` back to its yard spot over `captureAnim`, with `kill_alter.mp3`.
- **DiceView**: `dice1..6.png`. The roll animation cycles `diceroll1..6` every 0.035s for `diceRollAnim` seconds while scaling 1.0→1.3→1.0, then shows the face. Plays `dice_roll.mp3`. Only the human's dice is tappable, and only after `AwaitingRollMsg{isHuman}`.
- **RollChoiceView**: small dice chips (`dice{v}.png` at scale 0.3) in a row above the tapped token. Tapping a chip publishes `UiRollChosen`. Tapping elsewhere closes it.
- **Touch on tokens**: BoardView has one `EventListenerTouchOneByOne`. On touch end it hit-tests the highlighted TokenViews in reverse z-order using `getBoundingBox()` converted to BoardView space, then publishes `UiTokenTapped`.

### 9.4 Mission UI
- **MissionCardView** (224x120 panel with the `flat_round_white` 9-slice, tinted dark): title (bold 22), description (16, 2 lines, wrapped with `setDimensions`), a progress bar (DrawNode rect, redrawn), `progress/target`, "N turns left", and `+coins` with `coin.png`.
  - States: Offered (scale-in plus the "New mission!" toast), Progress (bar tween), Completed (green flash, coins fly to the counter, card removed after 0.8s), Failed (red flash plus shake, card removed after 0.8s), Voided (removed silently).
  - The HUD keeps slot order stable; a card is identified by `uid`.
- **CoinCounterView**: `coin.png` plus a number label. On `WalletChanged` it tweens the number, and pops if `delta > 0`.
- **ResultPopup**: a dimmed layer that swallows touches, the ranking list (colour dot plus name plus rank), "Missions completed: X", "Coins earned: +Y", and **Play again** / **Lobby** buttons that publish `UiResultClosed`.

### 9.5 Assets to copy (`scripts/copy_assets.sh`; source root `chaupar/StudioProject/AssetSrc/resizeables/`)

| Source | Destination (`Content/`) |
|---|---|
| `game/dice1..6.png`, `game/diceroll1..6.png` | `images/dice/` |
| `game/token_ludo_basic_base.png`, `game/token_ludo_basic_color.png`, `game/token_basic_shine.png` | `images/tokens/` |
| `game/star_outlined.png`, `game/box.png`, `game/triangle.png` | `images/board/` |
| `common/coin.png`, `common/big_coin.png`, `common/circle.png`, `common/sparkle_star.png` | `images/ui/` |
| `scale9/flat_round_white.png` | `images/ui/` |
| `chaupar/Resources/fonts/{mikado_bold.ttf, mikado_black.ttf, luckiest_guy.ttf}` | `fonts/` |
| `chaupar/Resources/sound/{dice_roll,token_move,kill_alter,my_turn,celebrate_win,coin_collect}.mp3` | `sounds/` |

The script uses `cp` and fails loudly (`set -euo pipefail`) if a source file is missing. The assets are small, so they **are committed**.

---

## 10. Phases

Every phase ends with the following checks:
- (a) `scripts/run_tests.sh` passes. This applies from P1 onward.
- (b) `scripts/build_mac.sh` succeeds.
- (c) The listed manual checks pass.
- (d) Commit on the working branch `feat/ludo-missions-mvp`, with message `P<N>: <summary>`.

**Do not start phase N+1 until phase N's checks are green.**

**How to hand a phase to a lower-tier model.** Save this plan as `docs/PLAN.md` in P0, then use this prompt:
> "Read `AGENTS.md` and `docs/PLAN.md` §1–§9 and §11. Implement **Phase N only**, exactly as specified; don't redesign anything. Run the phase's checks, fix anything that fails, and report the check output. Stop if a spec is ambiguous and ask."

Phases P2 and P5 are pure C++ with tests, so they are the safest to delegate. P3 and P7 are visual, so check them by eye.

Phase dependencies:
- P0 → P1 → P2 → P3 → P4.
- P5 → P5b. Both depend only on P1 and P2. **They can run in parallel with P3 and P4** in a separate worktree.
- P6 needs P4 and P5b. P7 needs P6. P8 needs P7.
- P5b is the most algorithmic phase. Give it to the strongest available model, or review it closely.

### Phase 0 — Repo and engine bootstrap
1. Replace `.gitignore` with:
   `/chaupar/`, `/axmol/`, `/build*/`, `.DS_Store`, `*.xcuserstate`, `compile_commands.json`, `/.cache/`.
   The current line `./chapuar` is misspelled, and gitignore does not support a `./` prefix.
2. `scripts/setup_engine.sh` (idempotent):
   `rsync -a --delete --exclude '/.git' --exclude '/tests/' --exclude '/docs/' chaupar/axmol/ axmol/`
   - **Exclude `.git`.** chaupar's `axmol/.git` is a *file* with `gitdir: ../.git/modules/axmol`, which would be a broken link after the copy.
   - The script also checks that `axmol/core/axmolver.h` exists and that `axmol/3rdparty/zlib/_x` exists (prebuilt deps are needed for an offline build).
   - It prints a clear error if `chaupar/axmol` is missing.
3. Copy the template:
   - `chaupar/axmol/templates/cpp/{CMakeLists.txt,Source,Content}` → repo root.
   - `chaupar/axmol/templates/common/proj.ios_mac` → `proj.ios_mac`. Don't copy `proj.android`, `proj.win32`, `proj.linux` or `proj.wasm`.
   - Delete `axproj-template.json` and the Windows `run.bat` lines in CMake.
4. Edit `CMakeLists.txt`. Make exact text replacements: `Dummy` → `LudoMissions`, and `org.axmol.dummy` → `com.gameberry.ludomissions`. Then:
   - Right after `project(${APP_NAME})`, add the following. Each line is a CACHE FORCE, placed **before** `include(AXBuildSet)`.
     ```cmake
     set(CMAKE_CXX_STANDARD 20)
     set(AX_EXT_HINT OFF CACHE BOOL "" FORCE)              # no extensions (none are used by core)
     set(AX_ENABLE_CONSOLE OFF CACHE BOOL "" FORCE)
     set(AX_UPDATE_BUILD_VERSION OFF CACHE BOOL "" FORCE)
     if(APPLE)
         set(AX_USE_COMPAT_GL ON CACHE BOOL "" FORCE)      # REQUIRED: OpenGL, like chaupar. The fork's Metal backend has an uninitialised TEXCOORD1 attrib -> broken sprites/crash
     endif()
     option(LM_DEV "Dev tooling (debug keys, live JSON from source dir)" ON)
     ```
   - **Do NOT turn off `AX_ENABLE_3D` or `AX_ENABLE_PHYSICS`.** `core/2d/Camera.h` includes `3d/Frustum.h` without a guard, so turning 3D off gives an undefined `ax::Plane::Plane()` at link time. chaupar never builds without them.
   - After `add_executable(...)` (note that `MACOSX` is only defined after `include(AXBuildSet)`), add:
     ```cmake
     if(MACOSX AND LM_DEV)
         target_compile_definitions(${APP_NAME} PRIVATE LM_DEV=1 LM_SOURCE_CONTENT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Content/")
     endif()
     ```
   - Includes are always written relative to `Source/` (the only include dir), e.g. `#include "Controllers/Logic/Rules.h"`. The template's `GLOB_RECURSE Source/*.cpp` already picks up subfolders, but you must **re-run the configure step after adding new files** (`build_mac.sh` always configures).
   - For iOS: change `set_xcode_property(${APP_NAME} TARGETED_DEVICE_FAMILY "1,2")` to `"1"` (iPhone only). With FIXED_WIDTH, an iPad would crop the top of a 720x1280 portrait layout.
5. Edit `Source/AppDelegate.cpp`:
   - Design size `720x1280`, `ResolutionPolicy::FIXED_WIDTH`.
   - **Keep** the existing `#if ... AX_PLATFORM_MAC` guard. Inside it: `GLViewImpl::createWithRect("LudoMissions", Rect(0,0,405,720))`. iOS keeps `GLViewImpl::create(...)`.
   - Set `director->setStatsDisplay(false)`.
   - Keep the template's `MainScene` for now.
6. iOS portrait: edit `proj.ios_mac/ios/targets/ios/Info.plist`. Set `UISupportedInterfaceOrientations` and `~ipad` to `UIInterfaceOrientationPortrait` only; the template ships **landscape**. In `RootViewController.mm`, make `supportedInterfaceOrientations` return `UIInterfaceOrientationMaskPortrait`.
7. `scripts/build_mac.sh`:
   ```sh
   #!/usr/bin/env bash
   set -euo pipefail; cd "$(dirname "$0")/.."; unset AX_ROOT   # shell AX_ROOT points to a DIFFERENT axmol (2.11.4)!
   [ -f axmol/core/axmolver.h ] || { echo "Run scripts/setup_engine.sh first"; exit 1; }
   cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES=arm64
   cmake --build build --config Debug --target LudoMissions -- -quiet
   ```
   - `scripts/run_mac.sh` runs `build_mac.sh`, then runs the binary **directly**, so `stderr` logs show in the terminal: `build/bin/LudoMissions/Debug/LudoMissions.app/Contents/MacOS/LudoMissions`. Using `open` would detach the logs. Check the path after the first build (it comes from `AXBuildHelpers.cmake:422`, `bin/${app_name}`, plus the Xcode config folder).
   - If CMake 4.4 rejects a policy, retry with `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`, or use `axmol/tools/external/cmake/bin/cmake` (3.30.5).
8. Copy `chaupar/.clang-format` to the repo. Write `AGENTS.md`, covering:
   - the layer rules (§3.1) and the event rules (§6.1)
   - the build/test commands
   - "never edit `axmol/`" and "never commit `chaupar/`"
   - "never switch to Metal"
   - "includes are relative to `Source/`"

   Write `CLAUDE.md` pointing to it.
9. `git checkout -b feat/ludo-missions-mvp` before the first commit. Add `.vscode/` to `.gitignore`.

**Checks:** a portrait window opens showing the template HelloWorld. `git status` shows no `axmol/` or `chaupar/`.

**Pitfalls:**
- The first configure is slow; the engine build takes about 5–10 minutes.
- `pwsh` must be on PATH (it is, 7.4.5), because `1k/fetch.cmake` runs `pwsh resolv-uri.ps1` at configure time.
- `rsync` excludes must be anchored (`--exclude '/tests/' --exclude '/docs/'`), so they don't remove nested folders called `tests` or `docs` inside `3rdparty`.

### Phase 1 — Foundations: EventBus, base views, config, wallet, scenes, test harness
1. `Utils/Log.h`: `#define LM_LOG(fmt, ...) std::fprintf(stderr, "[LM] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)`, and `LM_LOG_ERROR` the same way with a `"[LM][ERROR] "` prefix. `__VA_OPT__` lets zero-argument calls compile. They are pure, so they work in tests too.
2. `Utils/JsonUtils.{h,cpp}` (pure, rapidjson from `axmol/3rdparty/rapidjson`):
   - `bool parseJson(const std::string&, rapidjson::Document&, std::string& err)`, where `err` includes line:col computed from `GetErrorOffset()`.
   - Typed getters with defaults: `getInt(obj,"k",def)`, `getBool`, `getDouble`, `getString`.
3. `Events/EventBus.{h,cpp}` as in §6.1. Add the event headers with every struct from §6.2; later phases add fields if needed.
4. `Views/Common/BaseView` (`ax::Node`) and `BaseScene` (`ax::Scene`):
   - `onEnter() final { Node::onEnter(); initListeners(); afterEnter(); }`
   - `onExit() final { EventBus::unsubscribeAll(this); beforeExit(); Node::onExit(); }`
   - Create with `ax::utils::createInstance<T>()` and override `bool init() override`. `init()` **must be public**, because `createInstance` calls `&T::init`.
   - For views that take parameters, add a static `create(args)` that calls `new (std::nothrow) T()`, then `initWith(args)`, then `autorelease()`.
   - Children's `onEnter` runs inside `Node::onEnter()`, so **by the time `afterEnter()` runs, all child views are subscribed**. Publish "ready" events only from `afterEnter()`.
5. `Views/Common/UiFactory`: `makeLabel(text, size, bold)`, `makeButton(text, size, onClick)` (a `ui::Button` with `flat_round_white.png`, scale9, tinted green), and `makePanel(size, Color3B)`.
6. `Models/GameConfig.h`, plus `Controllers/Logic/ConfigParser` (pure) and `Content/config/game_config.json` (§8).
7. `Controllers/ConfigController` (singleton):
   - `readText(relPath)`: in `LM_DEV`, reads with `std::ifstream` from `LM_SOURCE_CONTENT_DIR + relPath` first, so JSON edits apply **without a rebuild**. Otherwise it uses `FileUtils::getInstance()->getStringFromFile(relPath)`.
   - `load()`: parses `game_config.json` and logs errors.
   - `const GameConfig& config()`.
   - Subscribes to `DebugReloadConfig`; it reloads and then publishes `ConfigReloaded{}`.
8. `Controllers/WalletController`:
   - `init()` reads `UserDefault::getInstance()->getIntegerForKey("lm.coins", 0)`.
   - `add(int coins, std::string reason)` writes the new value, calls `flush()`, and publishes `WalletChanged`.
   - Subscribes to `UiLobbyReady` and `UiGameSceneReady` and publishes `WalletChanged{balance, 0, "sync"}`.
   - Subscribes to `DebugResetCoins`.
9. `Controllers/SceneController`:
   - `UiPlayTapped` → `Director::replaceScene(GameScene)`.
   - `UiResultClosed{playAgain}` → a new GameScene, or LobbyScene.
   - `goToLobby()` is called by AppDelegate at boot (`runWithScene`).
10. `Views/Lobby/LobbyScene`: title, CoinCounterView, and a Play button that publishes `UiPlayTapped`. `afterEnter()` publishes `UiLobbyReady`.
11. `Views/Game/GameScene` placeholder: a label "Game" and a "Back" button (publishes `UiResultClosed{false}`). `afterEnter()` publishes `UiGameSceneReady`.
12. AppDelegate boot order, after the director and GLView are set:
    ```
    ConfigController::load() → WalletController::init() → SceneController::init() → (later) GameController::init() → MissionController::init() → runWithScene(Lobby)
    ```
    Delete the template's MainScene.
13. `tests/CMakeLists.txt`: a **standalone project**, which does NOT include axmol's CMake. Exact content:
    ```cmake
    cmake_minimum_required(VERSION 3.20)
    project(lm_tests CXX)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../Source)
    file(GLOB LM_TEST_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
    file(GLOB_RECURSE LM_LOGIC_SRC CONFIGURE_DEPENDS
         ${SRC_ROOT}/Models/*.cpp ${SRC_ROOT}/Controllers/Logic/*.cpp ${SRC_ROOT}/Utils/*.cpp)
    add_executable(lm_tests ${LM_TEST_SRC} ${LM_LOGIC_SRC})
    target_include_directories(lm_tests PRIVATE ${SRC_ROOT} ${CMAKE_CURRENT_SOURCE_DIR}/../axmol/3rdparty)
    enable_testing()
    add_test(NAME lm_tests COMMAND lm_tests)
    ```
    - `tests/test_main.cpp` contains exactly `#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN` followed by `#include "doctest/doctest.cpp"`. The engine's doctest is split: `doctest.h` only includes `doctest_fwd.h`, and the implementation lives in `doctest.cpp`.
    - Every other test file includes `"doctest/doctest.h"`.
    - `scripts/run_tests.sh`: `cmake -S tests -B build_tests -G Ninja && cmake --build build_tests && ./build_tests/lm_tests "$@"`.
14. First tests: `ConfigTests.cpp` covers defaults when keys are missing, overrides, and invalid JSON giving a line:col error.

**Checks:**
- The Lobby shows coins 0. Play goes to GameScene and Back returns to the Lobby.
- The tests pass.
- There are no listener leaks: go Lobby↔Game 10 times, and add a DEV log of `s_listeners.size()` in `unsubscribeAll`.

**Pitfalls:**
- The rapidjson include path in the app: axmol core already exposes `3rdparty` includes. If `#include "rapidjson/document.h"` fails in the app target, add `target_include_directories(${APP_NAME} PRIVATE ${_AX_ROOT}/3rdparty)`.
- A controller singleton is not a Node. Its subscriptions live until `reset()`, and that is fine.
- Never capture a raw `this` of a Node in a controller callback.

### Phase 2 — Pure game logic plus exhaustive tests (no rendering)
Implement §4 (BoardLayout), §5 (Rules, TurnMachine, BoardQueries, BotBrain, Rng), and `GameEvent::field` / `toString`.

`tests/TestHelpers.h` provides:
- `MatchState makeState(std::array<std::array<int,4>,4> progress, int current=0)`
- `std::vector<GameEventType> types(const std::vector<GameEvent>&)`

Test list:
- **Layout:** the adjacency test and the home-lane-entrance test from §4; the safe set; `globalCell` wrap.
- **Rules:**
  - Unlock only on 6.
  - Exact roll needed to finish (progress 53 + 4 is invalid, 53 + 3 is valid → 56).
  - No capture on safe cells.
  - No capture when 2 enemy tokens share the cell.
  - Capture of a single enemy.
  - Your own tokens stack.
  - `legalMoves` dedups roll values.
  - `autoMove` fires only with 1 token and 1 value.
- **TurnMachine:**
  - Roll 3 with all tokens in the yard → DICE_ROLLED, NO_MOVES, TURN_ENDED, TURN_STARTED(1).
  - Roll 6 → AwaitingRoll. Roll 4 → AwaitingMove with pending {6,4}. Use the 6 to unlock, then use the 4 → the turn ends.
  - Roll 6,6,6 with pending {6,6,6} → THREE_SIXES, then the turn ends.
  - Capture → BONUS_ROLL_GRANTED, and pending is preserved.
  - Finish → bonus roll.
  - Finishing the 4th token → PLAYER_FINISHED, TURN_ENDED, MATCH_ENDED, phase MatchOver, ranking has size 4.
  - Preconditions violated → an empty vector.
- **BotBrain:** prefers a capture over an unlock; prefers an escape; is deterministic with a fixed seed.
- **Simulation test:** 200 seeded all-bot matches all reach MatchOver within 5000 commands. This catches infinite loops.

**Checks:** all tests are green. Nothing under `Controllers/Logic` includes `axmol` or `ax/`; verify with `grep -rn "axmol\|#include \"ax" Source/Controllers/Logic Source/Models`, which should return nothing.

### Phase 3 — GameController plus a playable board
1. Run `scripts/copy_assets.sh` (§9.5).
2. **GameController** (singleton): owns `MatchState`, `TurnMachine`, `Rng` and `BotBrain`. It exposes `const MatchState& matchState() const` for read-only access by MissionController.
   - `UiGameSceneReady` → `newMatch()`:
     - Build the players from config.
     - `events = machine.startMatch()`.
     - Enqueue the events.
     - Publish `MatchSnapshot` immediately.
   - **Paced queue:** `std::deque<GameEvent> m_queue; float m_wait = 0;`, scheduled per frame with `scheduler->schedule([this](float dt){ tick(dt); }, this, 0.f, false, "lm.game.tick")`.
     - `tick`: if `m_wait > 0` then `m_wait -= dt` and return. Otherwise, if the queue is not empty: pop the front, publish `GameEventMsg`, set `m_wait = delayAfter(event)`, and return.
     - Otherwise, if the queue is empty and nothing has been prompted yet, call `promptCurrent()`.
     - `delayAfter`:

       | Event | Delay |
       |---|---|
       | DICE_ROLLED | diceRollAnim |
       | TOKEN_MOVED | steps × tokenStep |
       | TOKEN_CAPTURED | captureAnim |
       | TURN_ENDED | turnGap |
       | anything else | 0 |

     - Delays are multiplied by `fastBotsMultiplier` when the event's player is a bot and fastBots is on.
   - `promptCurrent()`, depending on the phase:
     - **AwaitingRoll**, human: publish `AwaitingRollMsg{isHuman:true}` and wait for `UiRollDiceTapped`.
     - **AwaitingRoll**, bot: publish `AwaitingRollMsg{false}` and schedule a roll after `botThinkDelay`.
     - **AwaitingMove**: publish `PendingRollsChanged`.
       - If `autoMove` exists, schedule it after `autoMoveDelay`.
       - Otherwise a human gets `AwaitingMoveMsg{options}`, and a bot gets a `BotBrain.choose` move scheduled after `botThinkDelay`.
     - **MatchOver**: stop ticking.
   - Guard every input with `m_inputLocked`:
     - Set it when a command is issued.
     - Clear it when the queue drains and the next prompt goes out.
     - Ignore `UiRollDiceTapped` and `UiTokenTapped` unless the current player is the human, the phase matches, and nothing is locked. **This prevents double-tap and double-move bugs.**
   - Dice value: `m_forcedNext != 0 ? consume : rng.dice()`. `DebugForceNextRoll` sets `m_forcedNext`.
   - `UiTokenTapped{token}`: take the legal values for that token.
     - 0 values: ignore.
     - 1 value: `move`.
     - More than 1: publish `RollChoiceRequested`.
   - `UiRollChosen` validates the value and then moves.
   - On `MATCH_ENDED`: after the queue drains, stop ticking; the view shows the result.
   - Leaving the scene (`UiResultClosed`, or the GameScene exiting) → `abortMatch()`: unschedule, clear the queue, `phase = NotStarted`.
   - **All delayed callbacks** use scheduler keys prefixed `lm.game.` and are cancelled in `abortMatch()`.
   - One-shot delay helper:
     ```cpp
     void GameController::after(float delay, const char* key, std::function<void()> fn) {
         auto* s = ax::Director::getInstance()->getScheduler();
         s->unschedule(key, this);   // REQUIRED: re-scheduling a pending key keeps the OLD callback (Scheduler.cpp:277-286)
         s->schedule([fn](float) { fn(); }, this, 0.f, 0, delay, false, key);
     }
     ```
   - Delayed callbacks **re-read state when they fire** (e.g. recompute `autoMove` or the bot choice then). Never capture a `MoveOption` at scheduling time. Each callback first checks that `m_matchId` (incremented on each new match) still matches the captured id.
   - **Wire it up in AppDelegate:** call `GameController::sharedController()->init()` after SceneController init.
   - GameController also subscribes to `UiGameSceneExiting` and calls `abortMatch()`.
3. **Views**:
   - `BoardGeometry`, `BoardView`, `TokenView`, `DiceView`, `PlayerPanelView`, `RollChoiceView` (per §9).
   - `GameScene` composes them. It keeps its own **view model**: a copy of the token progress, updated from `MatchSnapshot` and `GameEventMsg`.
   - Views react to `GameEventMsg` by `event.type`:
     - DICE_ROLLED → the dice animates.
     - TOKEN_MOVED → step animation along `BoardLayout::gridForToken`, for each progress from `from+1..to` (from −1, just one step to 0).
     - TOKEN_CAPTURED → the victim goes back to the yard.
     - TURN_STARTED → highlight the active panel and play `my_turn.mp3` if human.
     - MATCH_ENDED → after `resultPopupDelay`, show ResultPopup (basic ranking; the mission line shows 0 for now).
4. **Animation speed must match the controller's pacing:** views read durations from the same `GameConfig` values. They receive them via a `TimingConfig` copy in `MatchSnapshot`, so views don't include ConfigController.

**Checks (manual):**
- Play a full match vs 3 bots to the end.
- Stacked sixes show chips and work.
- A capture sends the victim home and grants a bonus roll.
- Tapping during bot turns does nothing.
- Play again works 3 times in a row with no crash and no duplicate moves.

**Pitfalls:**
- Don't animate from a stale view model: always apply `to` from the event.
- A token finishing moves to `FINISH_SPOT` plus a stack offset.
- TokenView z-order: raise the moving token to the top.
- Stop running actions on a token before starting a new move.

### Phase 4 — DEV tooling
- `DebugOverlayView` (added to GameScene only `#if LM_DEV`) uses `EventListenerKeyboard::onKeyPressed`:
  - `KEY_1..KEY_6` → `DebugForceNextRoll`.
  - `R` → `DebugReloadConfig`.
  - `F` → `DebugToggleFastBots`.
  - `C` → `DebugResetCoins`.
  - `M` → `DebugCycleForcedMission` (handled from P6).
- It shows a small label at the bottom-left, updated from `DebugStateChanged` / `MissionsReloaded`, e.g. "DEV · next roll: 6 · fast bots: on · force: capture_in_3".
- GameController handles `DebugForceNextRoll` (human's next roll only) and `DebugToggleFastBots`, then publishes `DebugStateChanged`.
- GameController applies `debug.startProgress` in `newMatch()` when it is non-empty and `LM_DEV`.
- MissionController (P6) also handles the reload.

**Checks:**
- Pressing 6 then tapping the dice rolls a 6; a bot roll in between does not consume it.
- Pressing F speeds up both bot pacing and bot animations.
- The Hunter scenario from §8 renders the tokens at the right cells.

### Phase 5 — Mission engine (pure) plus tests
Implement §7.4–§7.5:
- `Params`/`Spec`/`MissionDef`/`MissionInstance`/`MissionUpdate`
- `EventFilter`
- The registries and all built-ins in §7.2
- `MissionParser`, `TextTemplate`, `MissionEngine`

Tests:
- **Parser:**
  - Every starter mission loads with 0 errors.
  - Missing `id` → error.
  - Unknown condition type → error naming the id.
  - Unknown `where` field → error.
  - Unknown key → warning.
  - A JSON syntax error reports line:col.
  - A duplicate id → the second copy is rejected.
  - `enabled:false` → skipped.
- **EventFilter:** scalar, operators, `in`, the self/enemy/any roles.
- **Conditions:** each built-in on hand-built states, including `enemyAhead` blocked by a safe cell and by 2 enemies.
- **Objectives and engine:** everything in §7.6.
- **Integration:** load a JSON containing **only** `capture_in_3`, then run `TurnMachine` from the §8 Hunter `startProgress` through `MissionEngine`. Assert that it is offered on the first TURN_STARTED(0), and completes after `roll(3)` then `move(0,3)`.

P5 scope also includes:
- `MissionTracker`, used by the engine.
- The `moments` field plus the afterRoll offer logic.
- The `canCaptureNow` condition.
- `IOfferStrategy` plus `WeightedRandomStrategy`, which is the only strategy in P5.
- `Objective::clone/stateKey/minTurnsHint` for all built-ins, with tests that a clone is independent of the original.

**Checks:** tests are green. Adding a new mission to the test JSON needs no C++ change.

### Phase 5b — Mission Director (pure) plus tests
Implement §7.7: FeasibilitySearch, RolloutSimulator, UtilityScorer, DifficultyTracker, DirectorStrategy, and `sitsOut` support in `TurnMachine::endTurn` (with a test). The engine picks `DirectorStrategy` when `director.enabled`.

Tests (`DirectorTests.cpp`, all with `simBudgetMs = 0` and fixed seeds):
- **A\*:**
  - Hunter scenario (§8) + `capture_in_3` → Feasible, minTurns 0.
  - The enemy 20 cells ahead, `turns: 1`, no other tokens on the track → **Infeasible** (at most 17 cells per turn, because of the three-sixes forfeit).
  - The same with `turns: 2` → Feasible, minTurns 1.
  - A finish objective (TOKEN_FINISHED target 1) with the only out token at progress 5 and no enemies on the track needs 51 cells, so at 17 per turn it takes 3 turns:
    - `turns: 2` → **Infeasible**.
    - `turns: 3` → Feasible, minTurns 2 (it completes during the 3rd turn, before that turn's TURN_ENDED).
  - `avoid` → always Feasible.
  - A tiny budget (10 expansions) → Unknown, never Infeasible.
  - Deterministic: the same input gives the same expansions count.
- **Monte Carlo:**
  - `roll_six_now` at turn start → P ≈ 1/6 (±0.03 with 3000 rollouts).
  - `strike_now` when the pending roll captures → P = 1.0 (the policy always takes the capture).
  - `survive_4` with no enemies on the board → P = 1.0.
- **Utility:**
  - P equal to center beats P far from center.
  - `repeatPenalty` applies.
  - `minUtility` gate → no offer.
  - `temperature` 0.01 → argmax.
  - `behindBias` shifts center when the human is last.
- **DifficultyTracker:** steps and clamps.
- **Engine integration:**
  - The director never offers a mission that A\* marks Infeasible.
  - At afterRoll with a capturing roll pending, `strike_now` is served.
  - With `director.enabled=false`, it falls back to weighted random.
- **Perf (informational, no failing assert):** print the ms for a mid-game state with all 9 candidates, 96 rollouts, in a Release test build: `cmake -S tests -B build_tests_rel -DCMAKE_BUILD_TYPE=Release`. Target ≤ 12 ms. If it's slower, lower the default `rollouts`.

### Phase 6 — MissionController, wallet integration, starter content
1. `Content/config/missions.json` with the 8 starters (§7.3).
2. `Controllers/MissionController` (singleton). **Wire it up in AppDelegate:** `MissionController::sharedController()->init()` after GameController init.
   - `init()`: read the `missions.enabled` flag. If it is false, **do not subscribe to anything** (this is the modularity check).
   - Load the JSON via `ConfigController::readText`, log every error and warning, and call `setSettings`.
   - Subscribe to `GameEventMsg`:
     - On `MATCH_STARTED`, call `engine.startMatch(state.selfPlayer, seed)`.
     - For every event, `updates = engine.onEvent(e, GameController::sharedController()->matchState())`.
     - Publish each as `MissionUpdated`.
     - For `Completed`, call `WalletController::sharedController()->add(coins, "mission:" + id)`. This is a controller-to-controller call, which is allowed.
     - Track per-match counters.
     - On `MATCH_ENDED`, publish `MissionMatchSummary` **before** the void updates.
   - `DebugReloadConfig` → reload the JSON (only future offers change), log it, and publish `MissionsReloaded` for the DEV label.
   - `DebugCycleForcedMission` → cycle `engine.setForcedMission(id)`, then publish `DebugStateChanged`.
   - Director wiring:
     - Build the engine's strategy from `config().director`.
     - Restore the `DifficultyTracker` center from `UserDefault` key `lm.director.center` (when `persist`), and save it after every Completed/Failed.
     - Compute `OfferStats.humanRaceRank` from the total token progress in `matchState()`.
     - The engine seed = `debug.rngSeed` if non-zero, otherwise random.
     - `DebugResetCoins` also resets the difficulty center (handy for demos).
   - `UiGameSceneExiting` → `engine.endMatch()` (voids; publish the updates).
3. **Ordering note:** GameScene must not depend on the relative order of listeners.
   - It resets its cached `MissionMatchSummary` to zeros on every `MatchSnapshot`, then caches the latest summary whenever one arrives.
   - It builds ResultPopup `resultPopupDelay` seconds after MATCH_ENDED. By then the summary, which is published synchronously inside the same MATCH_ENDED dispatch, is always there.

**Checks (manual, with the debug keys):**
- Press M to force `roll_six_now`. A "Lucky Six" offer appears in the logs at turn start. Press 6, roll, and the log shows completion.
- Without forcing, `[LM][Director]` decision lines appear at every human turn start and after each roll, each finishing in ≤ 12 ms. The coin balance goes up and persists after restarting the app.
- `missions.enabled=false` → the game still plays and no mission logs appear.
- Edit `missions.json` (change the reward), press R, and the next offer uses the new value with no rebuild.

### Phase 7 — Mission UI
- MissionHudView plus MissionCardView (§9.4), subscribed to `MissionUpdated`.
- ToastView: a queued banner, 1.6s each, that never overlaps others.
- The coin-fly effect: 6 `coin.png` sprites with staggered `MoveTo` plus `EaseSineIn` from the card to the CoinCounterView world position, ending with `coin_collect.mp3`.
- ResultPopup mission section.

**Checks:**
- Drive every starter mission to completion and to failure with the debug keys.
- Three concurrent cards lay out correctly.
- A card vanishes on MATCH_ENDED with no coins.
- The counter animates.

**Pitfall:** the card position used for the coin-fly must be converted to world space (`convertToWorldSpace`), and then to the counter parent's space.

### Phase 8 — iOS plus docs plus polish
1. `scripts/build_ios.sh` (simulator):
   ```sh
   unset AX_ROOT; mkdir -p build_ios/runtime/axslc
   cmake -S . -B build_ios -G Xcode -DCMAKE_TOOLCHAIN_FILE=axmol/1k/ios.cmake -DARCHS=arm64 -DSIMULATOR=TRUE -DLM_DEV=OFF
   cmake --build build_ios --config Debug --target LudoMissions -- -sdk iphonesimulator
   ```
   - For a device, drop `SIMULATOR`, pass `-DCMAKE_XCODE_ATTRIBUTE_DEVELOPMENT_TEAM=<TEAM>`, and open `build_ios/LudoMissions.xcodeproj` to sign.
   - Fallback: the engine CLI: `AX_ROOT=$PWD/axmol pwsh axmol/tools/cmdline/axmol.ps1 build -p ios -a arm64 -sdk sim`.
   - **No iOS simulator runtime is installed on this machine** (`xcrun simctl list runtimes` is empty). Run once: `xcodebuild -downloadPlatform iOS`, which is a multi-GB download, so start it early, even during P3.
   - `scripts/run_ios_sim.sh`:
     ```sh
     xcrun simctl boot "iPhone 16" || true; open -a Simulator
     xcrun simctl install booted build_ios/bin/LudoMissions/Debug-iphonesimulator/LudoMissions.app   # verify path after first build
     xcrun simctl launch --console booted com.gameberry.ludomissions
     ```
     Pick the device name from `xcrun simctl list devices available`.
2. Check the safe area on a notched iPhone. Keep UI inside `Director::getSafeAreaRect()`; if needed, shift the top bar and HUD down by `safeArea` insets.
3. `docs/MISSIONS_GUIDE.md` (for designers):
   - The schema.
   - Every building block, with an example.
   - The event and field table.
   - How the turn window works.
   - Recipes for the 8 starters.
   - A "how to test a mission with debug keys" section.
   - "How to add a new building block" (for engineers): add a class to `BuiltinConditions.cpp`, add one register line, add a test.
4. `docs/ARCHITECTURE.md`: the layer diagram, the event catalogue, and why the game never knows about missions.

**Checks:** it runs in the iOS simulator in portrait; the tests are green; a full match completes on the simulator.

---

## 11. Global pitfalls checklist (read before every phase)

1. **Wrong engine:** `$AX_ROOT` in `~/.zshrc` points to stock axmol 2.11.4. The template prefers `./axmol` when it exists, but scripts still `unset AX_ROOT` because of the second glslcc search path.
2. **Metal:** `AX_USE_COMPAT_GL ON` on Apple is mandatory (the fork's Metal backend is broken for sprites). **3D and physics must stay ON** (the link fails otherwise).
3. **Broken gitlink:** never copy `chaupar/axmol/.git`.
4. **Fixed-priority 0 asserts:** always use priority 1.
5. **Listener leaks and dangling `this`:** Views unsubscribe in `onExit` (BaseView does it). Controllers unsubscribe in `reset()`. Never keep payload pointers.
6. **Publishing before subscribers exist:** "ready" events are published only from `afterEnter()`. GameController starts a match only on `UiGameSceneReady`.
7. **Re-entrancy:** handlers run synchronously inside `publish`. GameController must not issue machine commands from inside a `GameEventMsg` handler; it only enqueues and ticks.
8. **Double input:** the `m_inputLocked` plus phase checks in GameController.
9. **Hot-reload dangling:** active mission instances hold `shared_ptr<const CompiledMission>`.
10. **The pure layer stays pure:** no axmol includes in `Models/`, `Controllers/Logic/` or `Utils/`. The test build enforces this because it doesn't link axmol.
11. **DrawNode colours:** this fork uses `Color4B`.
12. **FileUtils caching in DEV:** we bypass it by reading source files with `std::ifstream` when `LM_DEV`.
13. **iOS orientation:** the template is landscape; fix both Info.plist keys and RootViewController.
14. **`turnsLeft` semantics:** it decrements on the human's TURN_ENDED only. TurnMachine emits TURN_ENDED in *every* turn-ending path, including NO_MOVES, THREE_SIXES and match end.
15. **STACK_AND_MOVE edge cases:** a bonus roll with non-empty pending rolls keeps them. After THREE_SIXES, remaining pending rolls (from before a bonus) are still playable.
16. **JSON strictness:** no comments or trailing commas; use `_note` fields.
17. **Scheduler keys:** unique per purpose, and cancelled on abort. A leftover bot callback after "Play again" means a move is made in the wrong match. Always `unschedule(key)` before `schedule(key)`, because re-scheduling a pending key keeps the old lambda. Guard each callback with `m_matchId`.
18. **Frozen text:** mission descriptions are re-rendered on every update, so `{turnsLeft}` stays live.
19. **Test CMake:** `add_executable` doesn't expand globs, so use `file(GLOB ...)`. doctest's `main` comes from `#include "doctest/doctest.cpp"` in `test_main.cpp` only.
20. **`EventBus::subscribe<T>` needs the explicit `<T>`.** `init()` must be public for `utils::createInstance`.
21. **Logs:** run the Mac binary directly (not with `open`), or the stderr logs are lost.
22. **Don't edit `axmol/`.** If an engine bug appears, work around it in game code.
23. **Director consistency:** the window/resolution rules exist **only** in `MissionTracker`. Never re-implement them in the search or the simulator, or P estimates will disagree with the real outcomes.
24. **Director determinism:** tests use `simBudgetMs = 0`. Use one seeded `Rng` and a fixed candidate order. Never use `std::unordered_map` iteration order in scoring or selection; use a vector or `std::map`.
25. **MatchState copy cost:** no strings or maps in MatchState. `sitsOut` must never leak into a real match. Assert it in `GameController::newMatch`.
26. **Object slicing:** `MissionTracker`'s copy constructor must `clone()` the objective. Copying a `unique_ptr` won't compile, and a shared pointer would silently share runtime state between A\* nodes.

---

## 12. Verification (end to end)

1. `scripts/run_tests.sh`: all doctest suites pass (layout, rules, turn machine, bot sim of 200 matches, parser, conditions, objectives, engine, director: A\*/Monte Carlo/utility/difficulty, integration).
2. `scripts/run_mac.sh`: Lobby → Play → full match vs 3 bots → Result popup → Play again → Lobby. The coin balance persists across app restarts.
3. Mission demo script (DEV keys):
   - Press `M` until "roll_six_now" is forced. At turn start "Lucky Six" appears. Press `6` and roll → the card completes and coins fly (+20).
   - Set `debug.startProgress` to the §8 Hunter scenario, force `capture_in_3` with `M`, and start a match: "Hunter" is offered. Press `3` and roll → capture → +50.
   - Edit a reward in `Content/config/missions.json`, press `R`, and it applies to the next offer.
   - With no forced mission, play normally and confirm a mix of missions appears and resolves. The `[LM][Director]` log shows P, minTurns and U for each decision. "Strike Now!" appears after a roll that can capture.
   - Complete 3 missions in a row and watch the log's `center` go down (harder offers). Fail 2 and watch it go up.
4. Set `missions.enabled=false` and relaunch: the game works, and no HUD cards or mission logs appear.
5. `scripts/build_ios.sh`, then run on the simulator in portrait.
6. `grep -rn "Controllers/" Source/Views` returns nothing (Views never include Controllers). `grep -rn "Mission" Source/Controllers/GameController.*` returns nothing.
