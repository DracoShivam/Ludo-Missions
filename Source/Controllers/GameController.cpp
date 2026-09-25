#include "Controllers/GameController.h"

#include <cstdlib>

#include "axmol.h"
#include "Controllers/ConfigController.h"
#include "Controllers/Logic/BotBrain.h"
#include "Controllers/Logic/Rules.h"
#include "Events/AppEvents.h"
#include "Events/DebugEvents.h"
#include "Events/EventBus.h"
#include "Events/GameEvents.h"
#include "Events/UiEvents.h"
#include "Utils/Log.h"

namespace lm {

GameController* GameController::sharedController() {
	static GameController* s_instance = new GameController();
	return s_instance;
}

void GameController::init() {
	m_fastBots = ConfigController::sharedController()->config().debug.fastBots;
#if defined(LM_DEV) && LM_DEV
	const char* ap = std::getenv("LM_AUTOPLAY");
	m_autoplay = ap && ap[0] == '1';
	if (m_autoplay) {
		m_fastBots = true;
		LM_LOG("DEV: autoplay enabled");
	}
#endif
	EventBus::subscribe<UiGameSceneReady>(this, [this](const UiGameSceneReady&) { newMatch(); });
	EventBus::subscribe<UiGameSceneExiting>(this, [this](const UiGameSceneExiting&) { abortMatch(); });
	EventBus::subscribe<UiRollDiceTapped>(this, [this](const UiRollDiceTapped&) {
		if (humanCanAct(Phase::AwaitingRoll)) {
			doRoll();
		}
	});
	EventBus::subscribe<UiTokenTapped>(this, [this](const UiTokenTapped& e) { onTokenTapped(e); });
	EventBus::subscribe<UiRollChosen>(this, [this](const UiRollChosen& e) { onRollChosen(e); });
	EventBus::subscribe<DebugForceNextRoll>(this, [this](const DebugForceNextRoll& e) {
		m_forcedRoll = e.value;
		publishDebugState();
	});
	EventBus::subscribe<DebugToggleFastBots>(this, [this](const DebugToggleFastBots&) {
		m_fastBots = !m_fastBots;
		publishDebugState();
	});
	EventBus::subscribe<ConfigReloaded>(this, [this](const ConfigReloaded&) {
		m_timing = ConfigController::sharedController()->config().timing;  // rules apply from the next match
	});
	ax::Director::getInstance()->getScheduler()->schedule([this](float dt) { tick(dt); }, this, 0.f, false, "lm.game.tick");
}

void GameController::newMatch() {
	abortMatch();
	const GameConfig& cfg = ConfigController::sharedController()->config();
	m_rules = cfg.rules;
	m_timing = cfg.timing;
	m_rng.seed(cfg.debug.rngSeed);
	m_state = MatchState{};
	for (int p = 0; p < NUM_PLAYERS; p++) {
		PlayerState ps;
		ps.color = p;
		ps.kind = (p == cfg.players.humanColor) ? PlayerKind::Human : PlayerKind::Bot;
		m_state.players.push_back(ps);
	}
	m_state.selfPlayer = cfg.players.humanColor;
#if defined(LM_DEV) && LM_DEV
	if (cfg.debug.startProgress.size() == 4) {
		for (int p = 0; p < 4; p++) {
			for (int t = 0; t < 4; t++) {
				m_state.players[p].progress[t] = cfg.debug.startProgress[p][t];
			}
		}
		LM_LOG("DEV: applied debug.startProgress scenario");
	}
#endif
	for (auto& p : m_state.players) {
		AX_ASSERT(!p.sitsOut);  // sitsOut is ONLY for the Mission Director's search
	}
	m_machine = std::make_unique<TurnMachine>(m_state, m_rules);
	auto events = m_machine->startMatch();

	MatchSnapshot snap;
	snap.state = m_state;
	snap.timing = m_timing;
	snap.names = cfg.players.names;
	EventBus::publish(snap);

	m_running = true;
	enqueue(events);
	publishDebugState();
	LM_LOG("match %d started", m_matchId);
}

void GameController::abortMatch() {
	m_matchId++;
	m_running = false;
	m_queue.clear();
	m_wait = 0.f;
	m_prompted = false;
	m_inputLocked = true;
	auto* s = ax::Director::getInstance()->getScheduler();
	for (const char* key : {"lm.game.bot", "lm.game.auto"}) {
		s->unschedule(key, this);
	}
	m_state.phase = Phase::NotStarted;
}

void GameController::enqueue(const std::vector<GameEvent>& events) {
	for (const auto& e : events) {
		m_queue.push_back(e);
	}
	m_prompted = false;
}

void GameController::tick(float dt) {
	if (!m_running) {
		return;
	}
	if (m_wait > 0.f) {
		m_wait -= dt;
		return;
	}
	if (!m_queue.empty()) {
		GameEvent e = m_queue.front();
		m_queue.pop_front();
		GameEventMsg msg;
		msg.event = e;
		msg.animScale = animScaleFor(e.player);
		int matchId = m_matchId;
		if (m_autoplay && e.type == GameEventType::TURN_STARTED && m_state.turnNumber % 40 == 0) {
			int sum = 0;
			for (auto& pl : m_state.players)
				for (int v : pl.progress) sum += v;
			LM_LOG("autoplay: turn %d, progress sum %d", m_state.turnNumber, sum);
		}
		if (e.type == GameEventType::MATCH_ENDED) {
			LM_LOG("match %d ended, winner %d", m_matchId, e.player);
			MatchRanking r;
			r.ranking = m_state.ranking;
			EventBus::publish(r);
		}
		EventBus::publish(msg);  // handlers run synchronously; they must not issue commands
		if (matchId != m_matchId) {
			return;  // aborted from inside a handler
		}
		m_wait = delayAfter(e) * msg.animScale;
		return;
	}
	if (!m_prompted) {
		m_prompted = true;
		promptCurrent();
	}
}

float GameController::delayAfter(const GameEvent& e) const {
	switch (e.type) {
		case GameEventType::DICE_ROLLED:
			return m_timing.diceRollAnim;
		case GameEventType::TOKEN_MOVED:
			return e.steps * m_timing.tokenStep;
		case GameEventType::TOKEN_CAPTURED:
			return m_timing.captureAnim;
		case GameEventType::TURN_ENDED:
			return m_timing.turnGap;
		default:
			return 0.f;
	}
}

bool GameController::isBot(int player) const {
	if (player < 0 || player >= (int) m_state.players.size()) return false;
	return m_autoplay || m_state.players[player].kind == PlayerKind::Bot;
}

float GameController::animScaleFor(int player) const {
	return (m_fastBots && isBot(player)) ? m_timing.fastBotsMultiplier : 1.f;
}

bool GameController::humanCanAct(Phase phase) const {
	return m_running && !m_inputLocked && m_prompted && m_queue.empty() && m_state.phase == phase &&
		   m_state.current == m_state.selfPlayer;
}

void GameController::promptCurrent() {
	int p = m_state.current;
	bool human = !isBot(p);
	float botDelay = m_timing.botThinkDelay * animScaleFor(p);
	int matchId = m_matchId;

	if (m_state.phase == Phase::MatchOver || m_state.phase == Phase::NotStarted) {
		m_running = false;
		return;
	}
	PendingRollsChanged pr;
	pr.player = p;
	pr.rolls = m_state.pendingRolls;
	EventBus::publish(pr);

	if (m_state.phase == Phase::AwaitingRoll) {
		AwaitingRollMsg msg;
		msg.player = p;
		msg.isHuman = human;
		EventBus::publish(msg);
		if (human) {
			m_inputLocked = false;
		} else {
			after(botDelay, "lm.game.bot", [this, matchId] {
				if (matchId == m_matchId && m_state.phase == Phase::AwaitingRoll) doRoll();
			});
		}
		return;
	}

	// AwaitingMove
	if (rules::autoMove(m_state)) {
		after(m_timing.autoMoveDelay * animScaleFor(p), "lm.game.auto", [this, matchId] {
			if (matchId != m_matchId || m_state.phase != Phase::AwaitingMove) return;
			if (auto o = rules::autoMove(m_state)) doMove(o->token, o->value);  // re-read state when firing
		});
		return;
	}
	if (human) {
		AwaitingMoveMsg msg;
		msg.player = p;
		msg.isHuman = true;
		msg.options = rules::legalMoves(m_state, p);
		EventBus::publish(msg);
		m_inputLocked = false;
	} else {
		after(botDelay, "lm.game.bot", [this, matchId] {
			if (matchId != m_matchId || m_state.phase != Phase::AwaitingMove) return;
			auto opts = rules::legalMoves(m_state, m_state.current);
			if (opts.empty()) return;
			MoveOption o = BotBrain::choose(m_state, opts, m_rng);
			doMove(o.token, o.value);
		});
	}
}

void GameController::doRoll() {
	int value = m_rng.dice();
	if (m_forcedRoll > 0 && m_state.current == m_state.selfPlayer) {
		value = m_forcedRoll;
		m_forcedRoll = 0;
		publishDebugState();
	}
	m_inputLocked = true;
	enqueue(m_machine->roll(value));
}

void GameController::doMove(int token, int value) {
	m_inputLocked = true;
	auto events = m_machine->move(token, value);
	if (events.empty()) {
		m_prompted = false;  // re-prompt
		return;
	}
	enqueue(events);
}

void GameController::onTokenTapped(const UiTokenTapped& e) {
	if (!humanCanAct(Phase::AwaitingMove) || e.player != m_state.selfPlayer) {
		return;
	}
	auto values = rules::legalValuesForToken(m_state, e.player, e.token);
	if (values.empty()) {
		return;
	}
	if (values.size() == 1) {
		doMove(e.token, values[0]);
		return;
	}
	RollChoiceRequested req;
	req.player = e.player;
	req.token = e.token;
	req.values = values;
	EventBus::publish(req);
}

void GameController::onRollChosen(const UiRollChosen& e) {
	if (!humanCanAct(Phase::AwaitingMove) || e.player != m_state.selfPlayer) {
		return;
	}
	auto values = rules::legalValuesForToken(m_state, e.player, e.token);
	if (std::find(values.begin(), values.end(), e.value) != values.end()) {
		doMove(e.token, e.value);
	}
}

void GameController::after(float delay, const char* key, std::function<void()> fn) {
	auto* s = ax::Director::getInstance()->getScheduler();
	s->unschedule(key, this);  // re-scheduling a pending key would keep the OLD callback
	s->schedule([fn](float) { fn(); }, this, 0.f, 0, delay, false, key);
}

void GameController::publishDebugState() {
	DebugStateChanged d;
	d.forcedRoll = m_forcedRoll;
	d.fastBots = m_fastBots;
	EventBus::publish(d);
}

}  // namespace lm
