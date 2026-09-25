#include "Controllers/GameController.h"

#include <cstdlib>

#include "axmol.h"
#include "Controllers/ConfigController.h"
#include "Controllers/Logic/Powers/PowerCatalog.h"
#include "Controllers/PowerController.h"
#include "Controllers/Logic/BotBrain.h"
#include "Controllers/Logic/Rules.h"
#include "Events/AppEvents.h"
#include "Events/DebugEvents.h"
#include "Events/EventBus.h"
#include "Events/PowerEvents.h"
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
	EventBus::subscribe<UiPowerTapped>(this, [this](const UiPowerTapped& e) { onPowerTapped(e.id); });
	EventBus::subscribe<UiPowerCancelled>(this, [this](const UiPowerCancelled&) { cancelTargeting(); });
	EventBus::subscribe<PowerInventoryChanged>(this, [this](const PowerInventoryChanged&) { publishPowerState(); });
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
		m_powers = ConfigController::sharedController()->config().powers;
	});
	ax::Director::getInstance()->getScheduler()->schedule([this](float dt) { tick(dt); }, this, 0.f, false, "lm.game.tick");
}

void GameController::newMatch() {
	abortMatch();
	const GameConfig& cfg = ConfigController::sharedController()->config();
	m_rules = cfg.rules;
	m_timing = cfg.timing;
	m_powers = cfg.powers;
	// GameController is a never-destroyed singleton, so anything match-scoped has to be cleared
	// here rather than merely initialised at construction -- that is how the old targeting state
	// survived "PLAY AGAIN" and kept the board dead.
	m_targeting.cancel();
	m_lastBotGrantTurn = 0;
	PowerController::sharedController()->resetMatch();
	m_rng.seed(cfg.debug.rngSeed);
	m_state = MatchState{};
	for (int p = 0; p < NUM_PLAYERS; p++) {
		PlayerState ps;
		ps.color = p;
		ps.kind = (p == cfg.players.humanColor) ? PlayerKind::Human : PlayerKind::Bot;
		m_state.players.push_back(ps);
	}
	m_state.selfPlayer = cfg.players.humanColor;
	// Sized here, AFTER the seats exist. Sizing it from m_state.players before the reset above left
	// it empty on the first match, so every tryBotPower bailed on the bounds check and bots silently
	// never used a power at all.
	m_botPowerTurn.assign(m_state.players.size(), -1);
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
	m_targeting.cancel();
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
		if (e.type == GameEventType::TOKEN_FINISHED && m_powers.enabled && e.player == m_state.selfPlayer) {
			PowerTier tier = PowerTier::Rare;
			powerTierFromString(m_powers.onTokenHomeTier, tier);
			PowerController::sharedController()->grantTier(e.player, tier, "home");
		}
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
	// A new prompt means a new turn or phase: targeting never survives one. This single line is the
	// difference between an armed power being a moment and being a permanent input trap.
	m_targeting.cancel();
	maybeGrantBotPower();
	float botDelay = m_timing.botThinkDelay * animScaleFor(p);
	int matchId = m_matchId;

	if (m_state.phase == Phase::MatchOver || m_state.phase == Phase::NotStarted) {
		m_running = false;
		publishTappable();
		publishPowerState();
		return;
	}
	PendingRollsChanged pr;
	pr.player = p;
	pr.rolls = m_state.pendingRolls;
	EventBus::publish(pr);
	publishPowerState();

	if (m_state.phase == Phase::AwaitingRoll) {
		AwaitingRollMsg msg;
		msg.player = p;
		msg.isHuman = human;
		EventBus::publish(msg);
		if (human) {
			m_inputLocked = false;
			publishTappable();
		} else {
			publishTappable();
			tryBotPower();  // start of a bot turn, before any dice are thrown
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
		publishTappable();
	} else {
		publishTappable();
		// Also here, not just before the roll: powers like Reroll are only legal once dice are on
		// the table, so a bot that could only act in AwaitingRoll would never use them at all.
		tryBotPower();
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
	if (m_targeting.active()) {
		// Every tap resolves the session: legal ones fire the power, anything else cancels. It is
		// never left armed, which is what made every later token tap disappear.
		const std::string armed = m_targeting.powerId();  // tap() clears the session; read it first
		// For a whole-player power, any of that player's tokens means "this player".
		PowerTarget tapped{e.player, e.token};
		if (m_targeting.targetKind() == TargetKind::OpponentPlayer) {
			tapped.token = -1;
		}
		auto r = m_targeting.tap(tapped);
		if (r == TargetingSession::TapResult::Applied) {
			usePower(armed, m_targeting.chosen());
		}
		publishTappable();
		publishPowerState();
		return;
	}
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

void GameController::onPowerTapped(const std::string& powerId) {
	// Every guard the move path has. The old targeting branch had none of these, which meant that
	// the moment the board let an enemy token through, powers could fire on a bot's turn.
	if (!m_running || powerId.empty() || m_inputLocked) {
		return;
	}
	if (m_state.current != m_state.selfPlayer || m_state.phase == Phase::MatchOver) {
		return;
	}
	auto* pc = PowerController::sharedController();
	if (pc->count(m_state.selfPlayer, powerId) <= 0) {
		return;
	}
	const CompiledPower* cp = pc->catalog().find(powerId);
	if (!cp) {
		return;
	}
	if (m_targeting.active() && m_targeting.powerId() == powerId) {
		cancelTargeting();  // tap the armed chip again to put it away
		return;
	}

	auto targets = cp->effect->targets({m_state, m_state.selfPlayer});
	if (targets.empty()) {
		return;  // the chip is already shown dimmed; nothing to do
	}
	if (cp->effect->targetKind() == TargetKind::None) {
		usePower(powerId, targets.front());  // nothing to choose
		return;
	}
	m_targeting.begin(powerId, cp->effect->targetKind(), std::move(targets));
	publishTappable();
	publishPowerState();
}

void GameController::cancelTargeting() {
	if (!m_targeting.active()) {
		return;
	}
	m_targeting.cancel();
	publishTappable();
	publishPowerState();
}

// What the player may tap right now. One publisher, so the board's highlights and the rules'
// notion of a legal target cannot drift apart.
void GameController::publishTappable() {
	TappableTokens msg;
	if (m_targeting.active()) {
		msg.reason = TappableTokens::Reason::PowerTarget;
		for (const auto& t : m_targeting.targets()) {
			if (t.token >= 0) {
				msg.tokens.push_back({t.player, t.token});
				continue;
			}
			// A whole-player target ({player, -1}) has no token of its own to point at. Light up all
			// four of that player's tokens and let any of them stand for the player -- otherwise the
			// power highlights nothing and cannot be aimed at all, which is what happened to Jinx.
			if (t.player >= 0) {
				for (int tok = 0; tok < TOKENS_PER_PLAYER; tok++) msg.tokens.push_back({t.player, tok});
			}
		}
		EventBus::publish(msg);
		return;
	}
	msg.reason = TappableTokens::Reason::Move;
	if (m_running && !m_inputLocked && m_state.phase == Phase::AwaitingMove && !isBot(m_state.current) &&
		m_state.current == m_state.selfPlayer) {
		for (const auto& o : rules::legalMoves(m_state, m_state.selfPlayer)) msg.tokens.push_back({o.player, o.token});
	}
	EventBus::publish(msg);
}

void GameController::publishPowerState() {
	PowerState st;
	auto* pc = PowerController::sharedController();
	int seat = m_state.selfPlayer;
	st.yourTurn = m_running && m_state.current == seat && m_state.phase != Phase::MatchOver;
	st.armedId = m_targeting.active() ? m_targeting.powerId() : "";

	for (const auto& id : pc->held(seat)) {
		const CompiledPower* cp = pc->catalog().find(id);
		if (!cp) continue;
		PowerChip chip;
		chip.id = id;
		chip.title = cp->def.title;
		chip.desc = cp->def.desc;
		chip.tier = cp->def.tier;
		chip.count = pc->count(seat, id);
		chip.usable = st.yourTurn && m_powers.enabled && !m_inputLocked &&
					  !cp->effect->targets({m_state, seat}).empty();
		if (id == st.armedId) st.armedHint = cp->def.desc;
		st.chips.push_back(chip);
	}
	EventBus::publish(st);
}

bool GameController::usePower(const std::string& powerId, PowerTarget target) {
	auto* pc = PowerController::sharedController();
	const CompiledPower* cp = pc->catalog().find(powerId);
	if (!cp) {
		return false;
	}
	// Charge BEFORE mutating. The old order applied the effect first and checked the charge after,
	// so a failed consume left the board changed with nothing emitted and nothing paid.
	if (!pc->consume(m_state.selfPlayer, powerId)) {
		return false;
	}
	auto events = cp->effect->apply(m_state, m_state.selfPlayer, target);
	if (events.empty()) {
		pc->grant(m_state.selfPlayer, powerId, "refund");  // effect declined: give it back
		return false;
	}
	m_targeting.cancel();
	LM_LOG("power used: %s", powerId.c_str());

	PowerUsedMsg msg;
	msg.id = powerId;
	msg.title = cp->def.title;
	msg.tier = cp->def.tier;
	msg.text = cp->def.desc;
	if (target.player >= 0 && target.player != m_state.selfPlayer) {
		const auto& names = ConfigController::sharedController()->config().players.names;
		if (target.player < (int) names.size()) {
			msg.text += "  (" + names[target.player] + ")";
		}
	}
	EventBus::publish(msg);

	enqueue(events);
	publishPowerState();
	return true;
}

void GameController::maybeGrantBotPower() {
	if (!m_powers.enabled || !m_powers.botsUsePowers || m_powers.botGrantEveryTurns <= 0) {
		return;
	}
	if (m_state.turnNumber - m_lastBotGrantTurn < m_powers.botGrantEveryTurns) {
		return;
	}
	m_lastBotGrantTurn = m_state.turnNumber;
	// Bots have no missions, so a turn timer is the only symmetric supply. Common tier keeps them
	// on a weaker diet than a player who is actually finishing missions.
	for (int p = 0; p < (int) m_state.players.size(); p++) {
		if (isBot(p) && m_state.players[p].finishRank == 0) {
			PowerTier tier = m_rng.unit() < m_powers.botRareChance ? PowerTier::Rare : PowerTier::Common;
			PowerController::sharedController()->grantTier(p, tier, "bot");
		}
	}
}

void GameController::tryBotPower() {
	if (!m_powers.enabled || !m_powers.botsUsePowers) {
		return;
	}
	int seat = m_state.current;
	if (seat < 0 || seat >= (int) m_botPowerTurn.size()) {
		return;
	}
	if (m_botPowerTurn[seat] == m_state.turnNumber) {
		return;  // already spent one this turn
	}
	auto* pc = PowerController::sharedController();

	// Collect everything spendable first, then pick at random. Walking held() in order meant the
	// bot always spent whichever power happened to sit first in powers.json -- 68% of all bot uses
	// were the same one, and the rest of the catalogue never appeared.
	std::vector<std::string> spendable;
	for (const auto& id : pc->held(seat)) {
		const CompiledPower* cp = pc->catalog().find(id);
		if (cp && !cp->effect->targets({m_state, seat}).empty()) {
			spendable.push_back(id);
		}
	}
	if (spendable.empty()) {
		return;
	}
	{
		const std::string id = spendable[m_rng.range(0, (int) spendable.size() - 1)];
		const CompiledPower* cp = pc->catalog().find(id);
		auto targets = cp->effect->targets({m_state, seat});
		// Simple and honest: bots spend what they can, aiming at the most advanced rival token when
		// the choice is theirs. Enough to keep the power economy symmetric without pretending to
		// be clever.
		PowerTarget best = targets.front();
		for (const auto& t : targets) {
			if (t.player < 0 || t.token < 0) continue;
			if (best.token < 0 || m_state.players[t.player].progress[t.token] > m_state.players[best.player].progress[best.token]) {
				best = t;
			}
		}
		if (!pc->consume(seat, id)) {
			return;
		}
		auto events = cp->effect->apply(m_state, seat, best);
		if (events.empty()) {
			pc->grant(seat, id, "refund");
			return;
		}
		m_botPowerTurn[seat] = m_state.turnNumber;
		LM_LOG("bot %d used power: %s (turn %d)", seat, id.c_str(), m_state.turnNumber);
		enqueue(events);
		return;
	}
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
