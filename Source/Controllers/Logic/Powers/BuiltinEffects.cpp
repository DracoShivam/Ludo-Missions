// The eight effect kinds. Each is a small class plus ONE reg.add(...) line at the bottom --
// that pair is the entire cost of teaching the game a new kind of power. Nine shipped powers
// use these eight kinds; dice_plus and dice_minus are the same kind with different params.

#include <algorithm>

#include "Controllers/Logic/Powers/EffectRegistry.h"
#include "Controllers/Logic/Rules.h"
#include "Models/BoardLayout.h"

namespace lm {

namespace {

GameEvent ev(GameEventType type, int player) {
	GameEvent e;
	e.type = type;
	e.player = player;
	return e;
}

bool onSharedTrack(int progress) {
	return progress >= 0 && progress <= LAST_TRACK_PROGRESS;
}

// Rivals still in the match. A finished or sat-out player is not a meaningful target.
std::vector<PowerTarget> livingOpponents(const EffectContext& ctx) {
	std::vector<PowerTarget> out;
	for (int p = 0; p < (int) ctx.state.players.size(); p++) {
		if (p == ctx.user || ctx.state.players[p].finishRank != 0 || ctx.state.players[p].sitsOut) continue;
		out.push_back({p, -1});
	}
	return out;
}

// Rival tokens that can be interfered with: on the shared track, not on a safe cell, owner
// not shielded. Shared by sendHome and swapTokens so the two cannot disagree about reach.
std::vector<PowerTarget> reachableOpponentTokens(const EffectContext& ctx, bool respectSafeCells) {
	std::vector<PowerTarget> out;
	for (int p = 0; p < (int) ctx.state.players.size(); p++) {
		if (p == ctx.user || ctx.state.players[p].shieldTurns > 0) continue;
		for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
			int prog = ctx.state.players[p].progress[t];
			if (!onSharedTrack(prog)) continue;
			if (respectSafeCells && board::isSafeCell(board::globalCell(p, prog))) continue;
			out.push_back({p, t});
		}
	}
	return out;
}

// --- self-targeted -----------------------------------------------------------------------

class DiceDeltaEffect : public IPowerEffect {
public:
	DiceDeltaEffect(int delta, bool onOpponent) : m_delta(delta), m_onOpponent(onOpponent) {}
	TargetKind targetKind() const override { return m_onOpponent ? TargetKind::OpponentPlayer : TargetKind::None; }
	std::vector<PowerTarget> targets(const EffectContext& ctx) const override {
		if (!m_onOpponent) {
			// Only before a roll. Using it after you have already thrown would silently defer the
			// bonus to some later roll, which reads as the power doing nothing.
			bool ok = ctx.state.phase == Phase::AwaitingRoll && ctx.state.players[ctx.user].diceDelta == 0;
			return ok ? std::vector<PowerTarget>{{-1, -1}} : std::vector<PowerTarget>{};
		}
		std::vector<PowerTarget> out;
		for (const auto& t : livingOpponents(ctx)) {
			if (ctx.state.players[t.player].diceDelta == 0) out.push_back(t);  // no stacking
		}
		return out;
	}
	std::vector<GameEvent> apply(MatchState& s, int user, PowerTarget target) const override {
		int who = m_onOpponent ? target.player : user;
		s.players[who].diceDelta = m_delta;
		GameEvent e = ev(GameEventType::POWER_USED, user);
		e.victimPlayer = m_onOpponent ? who : -1;
		e.value = m_delta;
		return {e};
	}

private:
	int m_delta;
	bool m_onOpponent;
};

class DiceForceEffect : public IPowerEffect {
public:
	explicit DiceForceEffect(int value) : m_value(value) {}
	TargetKind targetKind() const override { return TargetKind::None; }
	std::vector<PowerTarget> targets(const EffectContext& ctx) const override {
		if (ctx.state.phase != Phase::AwaitingRoll || ctx.state.players[ctx.user].forcedRoll != 0) {
			return {};
		}
		// Never hand the player the third six. Two sixes already on the table and forcing a third
		// forfeits the whole turn -- a power that actively hurts you is worse than no power.
		if (m_value == 6 && ctx.state.consecutiveSixes >= 2) {
			return {};
		}
		return {{-1, -1}};
	}
	std::vector<GameEvent> apply(MatchState& s, int user, PowerTarget) const override {
		s.players[user].forcedRoll = m_value;
		GameEvent e = ev(GameEventType::POWER_USED, user);
		e.value = m_value;
		return {e};
	}

private:
	int m_value;
};

class DiceRerollEffect : public IPowerEffect {
public:
	TargetKind targetKind() const override { return TargetKind::None; }
	std::vector<PowerTarget> targets(const EffectContext& ctx) const override {
		// Only meaningful once you have rolled and have not yet spent everything.
		return ctx.state.phase == Phase::AwaitingMove && !ctx.state.pendingRolls.empty() ? std::vector<PowerTarget>{{-1, -1}}
																						: std::vector<PowerTarget>{};
	}
	std::vector<GameEvent> apply(MatchState& s, int user, PowerTarget) const override {
		s.pendingRolls.clear();
		s.phase = Phase::AwaitingRoll;
		s.consecutiveSixes = 0;  // a rerolled throw starts a fresh six chain
		return {ev(GameEventType::POWER_USED, user)};
	}
};

class ImmunityEffect : public IPowerEffect {
public:
	explicit ImmunityEffect(int turns) : m_turns(turns) {}
	TargetKind targetKind() const override { return TargetKind::None; }
	std::vector<PowerTarget> targets(const EffectContext& ctx) const override {
		return ctx.state.players[ctx.user].shieldTurns == 0 ? std::vector<PowerTarget>{{-1, -1}} : std::vector<PowerTarget>{};
	}
	std::vector<GameEvent> apply(MatchState& s, int user, PowerTarget) const override {
		s.players[user].shieldTurns = m_turns;
		GameEvent e = ev(GameEventType::POWER_USED, user);
		e.value = m_turns;
		return {e};
	}

private:
	int m_turns;
};

class LeapToEffect : public IPowerEffect {
public:
	explicit LeapToEffect(int progress) : m_progress(progress) {}
	TargetKind targetKind() const override { return TargetKind::OwnToken; }
	std::vector<PowerTarget> targets(const EffectContext& ctx) const override {
		std::vector<PowerTarget> out;
		for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
			int prog = ctx.state.players[ctx.user].progress[t];
			if (prog < 0 || prog >= m_progress) continue;  // in the yard, or already past it
			out.push_back({ctx.user, t});
		}
		return out;
	}
	std::vector<GameEvent> apply(MatchState& s, int user, PowerTarget target) const override {
		int from = s.players[user].progress[target.token];
		// A leap lands like a move: it can capture, and it obeys every landing rule, because it
		// asks rules::capturableAt rather than deciding for itself.
		auto victim = rules::capturableAt(s, user, m_progress);
		s.players[user].progress[target.token] = m_progress;

		std::vector<GameEvent> out;
		GameEvent used = ev(GameEventType::POWER_USED, user);
		used.token = target.token;
		used.from = from;
		used.to = m_progress;
		out.push_back(used);

		GameEvent moved = ev(GameEventType::TOKEN_MOVED, user);
		moved.token = target.token;
		moved.from = from;
		moved.to = m_progress;
		moved.steps = m_progress - from;
		out.push_back(moved);

		if (victim) {
			s.players[victim->first].progress[victim->second] = IN_YARD;
			GameEvent cap = ev(GameEventType::TOKEN_CAPTURED, user);
			cap.token = target.token;
			cap.victimPlayer = victim->first;
			cap.victimToken = victim->second;
			cap.cell = board::globalCell(user, m_progress);
			out.push_back(cap);
		}
		return out;
	}

private:
	int m_progress;
};

// --- rival-targeted ----------------------------------------------------------------------

class SendHomeEffect : public IPowerEffect {
public:
	explicit SendHomeEffect(bool respectSafeCells) : m_respectSafe(respectSafeCells) {}
	TargetKind targetKind() const override { return TargetKind::OpponentToken; }
	std::vector<PowerTarget> targets(const EffectContext& ctx) const override {
		return reachableOpponentTokens(ctx, m_respectSafe);
	}
	std::vector<GameEvent> apply(MatchState& s, int user, PowerTarget target) const override {
		int cell = board::globalCell(target.player, s.players[target.player].progress[target.token]);
		s.players[target.player].progress[target.token] = IN_YARD;

		GameEvent used = ev(GameEventType::POWER_USED, user);
		used.victimPlayer = target.player;
		used.victimToken = target.token;
		used.cell = cell;
		// TOKEN_KICKED, deliberately not TOKEN_CAPTURED: a capture mission must not be completable
		// by spending an item a mission handed you. That loop would feed itself.
		GameEvent kicked = ev(GameEventType::TOKEN_KICKED, user);
		kicked.victimPlayer = target.player;
		kicked.victimToken = target.token;
		kicked.cell = cell;
		return {used, kicked};
	}

private:
	bool m_respectSafe;
};

class SwapTokensEffect : public IPowerEffect {
public:
	TargetKind targetKind() const override { return TargetKind::OpponentToken; }
	std::vector<PowerTarget> targets(const EffectContext& ctx) const override {
		// Needs one of ours on the track to trade places with, otherwise there is nothing to swap.
		bool haveOwn = false;
		for (int t = 0; t < TOKENS_PER_PLAYER; t++) haveOwn = haveOwn || onSharedTrack(ctx.state.players[ctx.user].progress[t]);
		return haveOwn ? reachableOpponentTokens(ctx, false) : std::vector<PowerTarget>{};
	}
	std::vector<GameEvent> apply(MatchState& s, int user, PowerTarget target) const override {
		// Swap with our LEAST advanced token on the track: the gain is largest and the choice needs
		// no second tap, which keeps the targeting flow to a single step.
		int mine = -1;
		for (int t = 0; t < TOKENS_PER_PLAYER; t++) {
			int prog = s.players[user].progress[t];
			if (!onSharedTrack(prog)) continue;
			if (mine < 0 || prog < s.players[user].progress[mine]) mine = t;
		}
		if (mine < 0) return {};

		int ours = s.players[user].progress[mine];
		int theirs = s.players[target.player].progress[target.token];
		s.players[user].progress[mine] = theirs;
		s.players[target.player].progress[target.token] = ours;

		GameEvent used = ev(GameEventType::POWER_USED, user);
		used.token = mine;
		used.from = ours;
		used.to = theirs;
		used.victimPlayer = target.player;
		used.victimToken = target.token;

		GameEvent moved = ev(GameEventType::TOKEN_MOVED, user);
		moved.token = mine;
		moved.from = ours;
		moved.to = theirs;
		moved.steps = theirs - ours;

		// The rival's token moved too. Emitting only our half left the board showing their token
		// where it used to be, with the model saying otherwise -- a desync that lasts until the
		// next full snapshot.
		GameEvent theirMove = ev(GameEventType::TOKEN_MOVED, target.player);
		theirMove.token = target.token;
		theirMove.from = theirs;
		theirMove.to = ours;
		theirMove.steps = ours - theirs;
		return {used, moved, theirMove};
	}
};

class SkipTurnEffect : public IPowerEffect {
public:
	explicit SkipTurnEffect(int turns) : m_turns(turns) {}
	TargetKind targetKind() const override { return TargetKind::OpponentPlayer; }
	std::vector<PowerTarget> targets(const EffectContext& ctx) const override {
		std::vector<PowerTarget> out;
		for (const auto& t : livingOpponents(ctx)) {
			if (ctx.state.players[t.player].skipTurns == 0) out.push_back(t);  // no stacking
		}
		return out;
	}
	std::vector<GameEvent> apply(MatchState& s, int user, PowerTarget target) const override {
		s.players[target.player].skipTurns = m_turns;
		GameEvent e = ev(GameEventType::POWER_USED, user);
		e.victimPlayer = target.player;
		e.value = m_turns;
		return {e};
	}

private:
	int m_turns;
};

}  // namespace

void registerBuiltinEffects(EffectRegistry& reg) {
	reg.add("diceDelta", {{"delta"}, {"who"}, [](const Spec& s, std::string& err) -> PowerEffectPtr {
		int delta = s.params.getInt("delta", 0);
		if (delta == 0) {
			err = "delta must be non-zero";
			return nullptr;
		}
		std::string who = s.params.getString("who", "self");
		if (who != "self" && who != "opponent") {
			err = "who must be \"self\" or \"opponent\"";
			return nullptr;
		}
		return std::make_shared<DiceDeltaEffect>(delta, who == "opponent");
	}});

	reg.add("diceForce", {{"value"}, {}, [](const Spec& s, std::string& err) -> PowerEffectPtr {
		int v = s.params.getInt("value", 0);
		if (v < 1 || v > 6) {
			err = "value must be 1..6";
			return nullptr;
		}
		return std::make_shared<DiceForceEffect>(v);
	}});

	reg.add("diceReroll", {{}, {}, [](const Spec&, std::string&) -> PowerEffectPtr { return std::make_shared<DiceRerollEffect>(); }});

	reg.add("immunity", {{"turns"}, {}, [](const Spec& s, std::string& err) -> PowerEffectPtr {
		int t = s.params.getInt("turns", 0);
		if (t < 1) {
			err = "turns must be >= 1";
			return nullptr;
		}
		return std::make_shared<ImmunityEffect>(t);
	}});

	reg.add("leapTo", {{"progress"}, {}, [](const Spec& s, std::string& err) -> PowerEffectPtr {
		int p = s.params.getInt("progress", -1);
		if (p < 1 || p > FINISHED) {
			err = "progress must be 1..56";
			return nullptr;
		}
		return std::make_shared<LeapToEffect>(p);
	}});

	reg.add("sendHome", {{}, {"respectSafeCells"}, [](const Spec& s, std::string&) -> PowerEffectPtr {
		return std::make_shared<SendHomeEffect>(s.params.getBool("respectSafeCells", true));
	}});

	reg.add("swapTokens", {{}, {}, [](const Spec&, std::string&) -> PowerEffectPtr { return std::make_shared<SwapTokensEffect>(); }});

	reg.add("skipTurn", {{}, {"turns"}, [](const Spec& s, std::string& err) -> PowerEffectPtr {
		int t = s.params.getInt("turns", 1);
		if (t < 1) {
			err = "turns must be >= 1";
			return nullptr;
		}
		return std::make_shared<SkipTurnEffect>(t);
	}});
}

}  // namespace lm
