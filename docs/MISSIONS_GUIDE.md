# Missions Guide

This guide is for game designers who want to add or tune missions. All missions live in `Content/config/missions.json`, and none of this work needs C++.

On a Mac DEV build the game reads that file straight from the source folder. To try a change:

1. Save the file.
2. Press **R** in-game.
3. Your change applies to the next mission the game offers.

If the file has a mistake, the terminal prints a precise message like `missions.json: mission 'capture_in_3': objective.where: unknown field 'playr'`. The broken mission is skipped and every other mission still loads.

---

## 1. Anatomy of a mission

```json
{
  "id": "capture_in_3",
  "title": "Hunter",
  "description": "Capture an enemy token within {turns} turns",
  "reward": { "coins": 50 },
  "turns": 3,
  "weight": 12,
  "cooldownTurns": 4,
  "maxPerMatch": 0,
  "moments": ["turnStart"],
  "offerWhen": { "type": "enemyAhead", "min": 1, "max": 6 },
  "objective": { "type": "count", "event": "TOKEN_CAPTURED", "where": { "player": "self" }, "target": 1 }
}
```

| Key | Required | Meaning |
|---|---|---|
| `id` | ✔ | A unique `snake_case` name. |
| `title` | ✔ | The short card title. |
| `description` | | The card text. Placeholders `{target}` `{turns}` `{turnsLeft}` `{progress}` update live. |
| `reward.coins` | ✔ | Coins paid when the mission is completed. |
| `turns` | ✔ | How many of **your** turns the player has. The count includes the turn in which the mission is offered, and bonus rolls do not use up a turn. |
| `weight` | | Default 10. How much the Director favours this mission (10 means neutral). |
| `cooldownTurns` | | Default from `game_config.json`. After the mission ends, it can't be offered again for this many turns. |
| `maxPerMatch` | | Default 0, which means unlimited. The maximum number of offers per match. |
| `moments` | | Default `["turnStart"]`. When the mission may be offered: `turnStart` (before rolling) and/or `afterRoll` (after the dice land, before moving). |
| `offerWhen` | | Default `always`. The board situation that makes this mission make sense. |
| `objective` | ✔ | What the player must do. |
| `enabled` | | Set to `false` to switch a mission off without deleting it. |
| `_note` | | A free-text comment. The game ignores it. JSON has no `//` comments, so use this instead. |

---

## 2. Objectives: what the player must do

| type | params | Behaviour | Example |
|---|---|---|---|
| `count` | `event`, `where`, `target` | Adds 1 for each matching event and completes at `target`. `target: 1` makes a one-shot mission. | Capture 2 tokens |
| `sum` | `event`, `where`, `field`, `target` | Adds up a number field of each matching event. | Move 25 cells (`field: "steps"`) |
| `streak` | `event`, `where`, `target` | Counts **consecutive turns** that each contain at least one matching event. A turn without one resets the count, and the mission fails early once it can no longer be won. | Roll a 6 two turns in a row |
| `avoid` | `event`, `where` | Fails the moment a matching event happens. Completes when the turns run out. | Don't get captured for 4 turns |
| `state` | `cond` | Completes as soon as the board condition becomes true. It is never offered if the condition is already true. | Have 3 tokens out of the yard |

### Events and their fields (used by `event`, `where` and `field`)

| Event | Fields you can filter on |
|---|---|
| `DICE_ROLLED` | `player`, `value` |
| `TOKEN_MOVED` | `player`, `token`, `from`, `to`, `steps`, `value` |
| `TOKEN_UNLOCKED` | `player`, `token` (leaving the yard) |
| `TOKEN_ENTERED_HOME_LANE` | `player`, `token` |
| `TOKEN_CAPTURED` | `player` (the capturer), `victimPlayer`, `victimToken`, `cell` |
| `TOKEN_FINISHED` | `player`, `token` |
| `BONUS_ROLL_GRANTED` | `player`, `reason` (1 = capture, 2 = finish) |
| `THREE_SIXES`, `NO_MOVES`, `TURN_STARTED`, `TURN_ENDED` | `player` |

Progress values are the cells a token has travelled from its own start: −1 means in the yard, 0–50 is the shared track, 51–55 is the home lane, and 56 means finished. An unlock move counts as 1 step.

### `where` filters

Every entry in a `where` filter must match. You can use:
- **An exact value:** `"value": 6`
- **A comparison:** `"steps": { "gte": 5 }`, using one of `eq`, `ne`, `lt`, `lte`, `gt`, `gte`, or `{"in": [1, 6]}`
- **A role, for `player` and `victimPlayer`:** `"self"` (the human), `"enemy"` (any bot) or `"any"`

---

## 3. Conditions: when a mission makes sense (`offerWhen` and `state`)

| type | params | True when |
|---|---|---|
| `always` | – | always |
| `all` / `any` | `of: [ ... ]` | all / any of the child conditions are true |
| `not` | `cond: { ... }` | the child condition is false |
| `enemyAhead` | `min`, `max` | one of your tokens can land on a lone enemy token that is `min`–`max` cells ahead, on a cell that isn't safe |
| `enemyBehind` | `min`, `max` | a lone enemy token sits `min`–`max` cells behind one of your tokens, which is on a cell that isn't safe |
| `tokenCount` | `zone`, `who`, `op`, `value` | the number of tokens in `yard`/`track`/`homeLane`/`finished`/`outOfYard`, for `self` or all `enemy` players, compares true |
| `tokenProgressAtLeast` | `value` | one of your unfinished tokens has progress ≥ `value` |
| `selfTurnNumber` | `op`, `value` | your turn counter compares true (1 is your first turn) |
| `canCaptureNow` | – | the dice you just rolled can capture right now. Use it with `"moments": ["afterRoll"]`. |

---

## 4. How the game chooses a mission (the Mission Director)

Each time a moment happens, the Director gathers every mission that is eligible: its moment matches, its `offerWhen` is true, it is off cooldown, it isn't already active, and there is a free slot (3 at most). It then scores each candidate in three steps:

1. **A\* feasibility.** Assuming the best possible dice and bots that don't move, can this mission still be finished in time? If not, it is dropped. For example, capturing a token 20 cells away in one turn is impossible, because three 6s forfeit the turn and a turn moves at most 17 cells.
2. **Monte Carlo.** It plays about 96 quick simulated futures with real random dice and real bots, and measures the probability P that the player succeeds.
3. **Utility.** `weight/10 × difficulty fit × timeliness × novelty`.
   - **Difficulty fit** peaks when P is near the player's current **target difficulty**. The target starts at 0.5. Completing a mission lowers it (harder missions next), and failing raises it (easier missions next). A player in last place gets easier missions.
   - **Timeliness** gives a bonus to missions that can be finished this very turn.
   - **Novelty** favours missions not seen recently.

It then picks among the best candidates with a little randomness. If none of them scores well enough (`director.utility.minUtility`), **it offers nothing**, because a bad mission is worse than no mission.

With the DEV build running in a terminal, every decision is printed:

```
[LM] [Director] turnStart center=0.45 8.9ms | three_out_4 P=0.26 minT=0 U=0.24 | six_streak_2 P=0.01 minT=1? U=0.06 PICK
```

Read it like this:
- `P` is the simulated success chance.
- `minT` is the best-case number of turns needed. A `?` means the search ran out of budget, so the mission was kept to be safe.
- `U` is the utility score.
- `PICK` marks the mission that was offered.

All the Director's knobs live in `game_config.json` under `director`.

---

## 5. Testing a mission quickly (DEV keys, macOS)

| Key | Effect |
|---|---|
| `1`–`6` | Your next roll will be this value. |
| `M` | Cycles through the missions. The chosen one is forced whenever it is eligible. |
| `R` | Reloads `missions.json` and `game_config.json`. |
| `F` | Speeds the bots up. |
| `C` | Resets coins and the difficulty target. |

To set up a board position, fill in `debug.startProgress` in `game_config.json` with a 4×4 grid of token progress values (rows are players: RED, GREEN, YELLOW, BLUE). For example, this sets up a "Hunter" demo with an enemy exactly 3 ahead:

```json
"startProgress": [[14,-1,-1,-1],[4,-1,-1,-1],[-1,-1,-1,-1],[-1,-1,-1,-1]]
```

Start a match, press `M` until `capture_in_3` is forced, press `3`, then roll.

To watch the game play itself, launch with `LM_AUTOPLAY=1 scripts/run_mac.sh`. The bot brain then plays your seat too, and you can watch the mission log.

---

## 6. For engineers: adding a new building block

- **A new condition:** add a small `Condition` struct plus one `reg.add("name", {required, optional, factory})` line in `Source/Controllers/Logic/Missions/BuiltinConditions.cpp`. Add a test in `tests/`.
- **A new objective shape:** do the same in `BuiltinObjectives.cpp`. Implement `clone()`. If the objective has internal state, also implement `stateKey()`. If you can prove a lower bound on turns, `minTurnsHint()` makes the A\* search faster.
- **A new event field:** add it to `GameEvent`, `GameEvent::field()` and `FIELD_NAMES`, and emit it from `TurnMachine`.

Nothing else changes: the parser, validation, engine and Director pick the new block up automatically.
