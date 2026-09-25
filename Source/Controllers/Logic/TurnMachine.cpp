#include "Controllers/Logic/TurnMachine.h"

#include <algorithm>

#include "Controllers/Logic/Rules.h"
#include "Models/BoardLayout.h"
#include "Utils/Log.h"

namespace lm {

TurnMachine::TurnMachine(MatchState& state, const RulesConfig& rules) : m_state(state), m_rules(rules) {
}

GameEvent TurnMachine::ev(GameEventType type) const {
	GameEvent e;
	e.type = type;
	e.player = m_state.current;
	return e;
}

std::vector<GameEvent> TurnMachine::startMatch() {
	std::vector<GameEvent> out;
	m_state.current = 0;
	m_state.turnNumber = 1;
	m_state.pendingRolls.clear();
	m_state.consecutiveSixes = 0;
	m_state.bonusRollPending = false;
	m_state.ranking.clear();
	out.push_back(ev(GameEventType::MATCH_STARTED));
	out.push_back(ev(GameEventType::TURN_STARTED));
	m_state.phase = Phase::AwaitingRoll;
	return out;
}

std::vector<GameEvent> TurnMachine::roll(int value) {
	std::vector<GameEvent> out;
	if (m_state.phase != Phase::AwaitingRoll || value < 1 || value > 6) {
		LM_LOG_ERROR("TurnMachine::roll(%d) rejected (phase=%d)", value, (int) m_state.phase);
		return out;
	}
	m_state.bonusRollPending = false;
	m_state.pendingRolls.push_back(value);
	GameEvent rolled = ev(GameEventType::DICE_ROLLED);
	rolled.value = value;
	out.push_back(rolled);

	if (value == 6) {
		m_state.consecutiveSixes++;
		if (m_rules.threeSixesForfeit && m_state.consecutiveSixes == 3) {
			for (int i = 0; i < 3 && !m_state.pendingRolls.empty(); i++) {
				m_state.pendingRolls.pop_back();
			}
			out.push_back(ev(GameEventType::THREE_SIXES));
			m_state.consecutiveSixes = 0;
			if (m_state.pendingRolls.empty()) {
				endTurn(out);
				return out;
			}
			resolveMovesOrEndTurn(out);
			return out;
		}
		m_state.phase = Phase::AwaitingRoll;  // roll again before moving
		return out;
	}
	m_state.consecutiveSixes = 0;
	resolveMovesOrEndTurn(out);
	return out;
}

void TurnMachine::resolveMovesOrEndTurn(std::vector<GameEvent>& out) {
	if (rules::legalMoves(m_state, m_state.current).empty()) {
		out.push_back(ev(GameEventType::NO_MOVES));
		m_state.pendingRolls.clear();
		endTurn(out);
	} else {
		m_state.phase = Phase::AwaitingMove;
	}
}

std::vector<GameEvent> TurnMachine::move(int token, int value) {
	std::vector<GameEvent> out;
	if (m_state.phase != Phase::AwaitingMove || token < 0 || token >= TOKENS_PER_PLAYER) {
		LM_LOG_ERROR("TurnMachine::move(%d,%d) rejected (phase=%d)", token, value, (int) m_state.phase);
		return out;
	}
	auto rollIt = std::find(m_state.pendingRolls.begin(), m_state.pendingRolls.end(), value);
	PlayerState& me = m_state.players[m_state.current];
	int from = me.progress[token];
	int to = rules::targetProgress(from, value);
	if (rollIt == m_state.pendingRolls.end() || to == INVALID_PROGRESS) {
		LM_LOG_ERROR("TurnMachine::move(%d,%d) illegal", token, value);
		return out;
	}
	m_state.pendingRolls.erase(rollIt);
	auto victim = rules::capturableAt(m_state, m_state.current, to);  // check BEFORE moving (own token not on target yet)
	me.progress[token] = to;

	GameEvent moved = ev(GameEventType::TOKEN_MOVED);
	moved.token = token;
	moved.from = from;
	moved.to = to;
	moved.steps = (from == IN_YARD) ? 1 : to - from;
	moved.value = value;
	out.push_back(moved);

	GameEvent tokenEv = ev(GameEventType::TOKEN_MOVED);
	tokenEv.token = token;
	tokenEv.from = from;
	tokenEv.to = to;
	if (from == IN_YARD) {
		tokenEv.type = GameEventType::TOKEN_UNLOCKED;
		out.push_back(tokenEv);
	}
	if (from <= LAST_TRACK_PROGRESS && to >= HOME_LANE_FIRST && to <= HOME_LANE_LAST) {
		tokenEv.type = GameEventType::TOKEN_ENTERED_HOME_LANE;
		out.push_back(tokenEv);
	}

	int bonusReason = BONUS_NONE;
	if (victim) {
		m_state.players[victim->first].progress[victim->second] = IN_YARD;
		GameEvent cap = ev(GameEventType::TOKEN_CAPTURED);
		cap.token = token;
		cap.victimPlayer = victim->first;
		cap.victimToken = victim->second;
		cap.cell = board::globalCell(m_state.current, to);
		out.push_back(cap);
		if (m_rules.captureBonusRoll) {
			m_state.bonusRollPending = true;
			bonusReason = BONUS_CAPTURE;
		}
	}

	if (to == FINISHED) {
		tokenEv.type = GameEventType::TOKEN_FINISHED;
		out.push_back(tokenEv);
		if (m_rules.finishBonusRoll) {
			m_state.bonusRollPending = true;
			bonusReason = BONUS_FINISH;
		}
		bool allHome = std::all_of(me.progress.begin(), me.progress.end(), [](int p) { return p == FINISHED; });
		if (allHome) {
			me.finishRank = 1;
			GameEvent pf = ev(GameEventType::PLAYER_FINISHED);
			pf.rank = 1;
			out.push_back(pf);
			endMatch(m_state.current, out);
			return out;
		}
	}

	if (m_state.bonusRollPending) {
		GameEvent bonus = ev(GameEventType::BONUS_ROLL_GRANTED);
		bonus.reason = bonusReason;
		out.push_back(bonus);
		m_state.phase = Phase::AwaitingRoll;  // pending rolls are KEPT: roll first, then spend everything
		return out;
	}
	if (!m_state.pendingRolls.empty()) {
		resolveMovesOrEndTurn(out);
		return out;
	}
	endTurn(out);
	return out;
}

void TurnMachine::endTurn(std::vector<GameEvent>& out) {
	out.push_back(ev(GameEventType::TURN_ENDED));
	int n = (int) m_state.players.size();
	int next = m_state.current;
	for (int i = 1; i <= n; i++) {
		int cand = (m_state.current + i) % n;
		const PlayerState& p = m_state.players[cand];
		if (p.finishRank == 0 && !p.sitsOut) {
			next = cand;
			break;
		}
	}
	m_state.current = next;
	m_state.pendingRolls.clear();
	m_state.consecutiveSixes = 0;
	m_state.bonusRollPending = false;
	m_state.turnNumber++;
	out.push_back(ev(GameEventType::TURN_STARTED));
	m_state.phase = Phase::AwaitingRoll;
}

void TurnMachine::endMatch(int winner, std::vector<GameEvent>& out) {
	std::vector<int> others;
	for (int p = 0; p < (int) m_state.players.size(); p++) {
		if (p != winner) {
			others.push_back(p);
		}
	}
	std::stable_sort(others.begin(), others.end(), [this](int a, int b) {
		return rules::totalProgress(m_state.players[a]) > rules::totalProgress(m_state.players[b]);
	});
	m_state.ranking.clear();
	m_state.ranking.push_back(winner);
	for (size_t i = 0; i < others.size(); i++) {
		m_state.ranking.push_back(others[i]);
		m_state.players[others[i]].finishRank = (int) i + 2;
	}
	m_state.pendingRolls.clear();
	m_state.phase = Phase::MatchOver;
	out.push_back(ev(GameEventType::TURN_ENDED));
	GameEvent end = ev(GameEventType::MATCH_ENDED);
	end.player = winner;
	out.push_back(end);
}

}  // namespace lm
