#pragma once

#include <deque>
#include <functional>
#include <memory>

#include "Controllers/Logic/Rng.h"
#include "Controllers/Logic/Powers/TargetingSession.h"
#include "Controllers/Logic/TurnMachine.h"
#include "Models/GameConfig.h"
#include "Models/MatchState.h"

namespace lm {

struct UiTokenTapped;
struct UiRollChosen;

// Owns the match. Runs the pure TurnMachine, paces its events for the views (docs/PLAN.md §10 P3), drives bots.
// Knows NOTHING about missions.
class GameController {
public:
	static GameController* sharedController();
	void init();
	const MatchState& matchState() const { return m_state; }

private:
	GameController() = default;
	void newMatch();
	void abortMatch();
	void tick(float dt);
	void enqueue(const std::vector<GameEvent>& events);
	void promptCurrent();
	void doRoll();
	void doMove(int token, int value);
	float delayAfter(const GameEvent& e) const;
	float animScaleFor(int player) const;
	bool isBot(int player) const;
	bool humanCanAct(Phase phase) const;
	void onTokenTapped(const UiTokenTapped& e);
	void onPowerTapped(const std::string& powerId);
	void cancelTargeting();
	void publishPowerState();
	void publishTappable();
	bool usePower(const std::string& powerId, PowerTarget target);
	void maybeGrantBotPower();
	void tryBotPower();
	void onRollChosen(const UiRollChosen& e);
	void after(float delay, const char* key, std::function<void()> fn);
	void publishDebugState();

	MatchState m_state;
	std::unique_ptr<TurnMachine> m_machine;
	RulesConfig m_rules;
	TimingConfig m_timing;
	Rng m_rng;
	std::deque<GameEvent> m_queue;
	float m_wait = 0.f;
	bool m_running = false;
	bool m_prompted = false;
	bool m_inputLocked = true;
	int m_matchId = 0;
	int m_forcedRoll = 0;
	bool m_fastBots = false;
	PowersConfig m_powers;
	// Armed-and-choosing state. A pure object so every transition is unit-tested; see
	// TargetingSession for why that matters here specifically.
	TargetingSession m_targeting;
	int m_lastBotGrantTurn = 0;
	// Turn number on which each seat last spent a power. Spending one enqueues events, which sets
	// m_prompted = false, which re-enters promptCurrent once the queue drains -- so without this a
	// bot re-enters tryBotPower and empties its whole inventory in a single turn.
	std::vector<int> m_botPowerTurn;
	bool m_autoplay = false;  // DEV: env LM_AUTOPLAY=1 -> the BotBrain also plays the human seat (soak tests)
};

}  // namespace lm
