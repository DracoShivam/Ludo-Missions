// Headless match tracer.
//
// Plays a full match with the real TurnMachine, the real BotBrain and the real mission engine + Director, and writes a
// JSON trace of every event: board state, dice, mission offers and progress, coins. Nothing here is a mock -- the trace
// is what the game would actually have done with this seed.
//
//   scripts/trace_match.sh [seed] [out.json]
//
// Used to render a replay outside the engine, and useful on its own for debugging a mission that behaves oddly.

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Controllers/Logic/BotBrain.h"
#include "Controllers/Logic/Missions/Director/DirectorStrategy.h"
#include "Controllers/Logic/Missions/MissionEngine.h"
#include "Controllers/Logic/Rng.h"
#include "Controllers/Logic/Rules.h"
#include "Controllers/Logic/TurnMachine.h"
#include "Models/BoardLayout.h"

using namespace lm;

namespace {

std::string readFile(const std::string& path) {
	std::ifstream in(path);
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

std::string esc(const std::string& s) {
	std::string out;
	for (char c : s) {
		if (c == '"' || c == '\\') {
			out += '\\';
			out += c;
		} else if (c == '\n') {
			out += "\\n";
		} else {
			out += c;
		}
	}
	return out;
}

std::string gridPair(GridPos g) {
	char buf[64];
	std::snprintf(buf, sizeof(buf), "[%.1f,%.1f]", g.col, g.row);
	return buf;
}

std::string layoutJson() {
	std::string s = "{\"track\":[";
	for (int i = 0; i < TRACK_LEN; i++) s += (i ? "," : "") + gridPair(board::trackGrid(i));
	s += "],\"homeLane\":[";
	for (int c = 0; c < NUM_PLAYERS; c++) {
		s += (c ? ",[" : "[");
		for (int i = 0; i < 5; i++) s += (i ? "," : "") + gridPair(board::homeLaneGrid(c, i));
		s += "]";
	}
	s += "],\"yard\":[";
	for (int c = 0; c < NUM_PLAYERS; c++) {
		s += (c ? ",[" : "[");
		for (int t = 0; t < TOKENS_PER_PLAYER; t++) s += (t ? "," : "") + gridPair(board::yardSpotGrid(c, t));
		s += "]";
	}
	s += "],\"finish\":[";
	for (int c = 0; c < NUM_PLAYERS; c++) s += (c ? "," : "") + gridPair(board::finishGrid(c));
	s += "],\"safeCells\":[";
	bool first = true;
	for (int i = 0; i < TRACK_LEN; i++) {
		if (!board::isSafeCell(i)) continue;
		s += (first ? "" : ",") + std::to_string(i);
		first = false;
	}
	s += "],\"startCells\":[";
	for (int c = 0; c < NUM_PLAYERS; c++) s += (c ? "," : "") + std::to_string(board::startCell(c));
	s += "]}";
	return s;
}

std::string missionsJson(const std::vector<MissionInstance>& ms) {
	std::string s = "[";
	for (size_t i = 0; i < ms.size(); i++) {
		const MissionInstance& m = ms[i];
		s += (i ? "," : "");
		s += "{\"uid\":" + std::to_string(m.uid) + ",\"id\":\"" + esc(m.id) + "\",\"title\":\"" + esc(m.title) + "\",\"desc\":\"" +
			 esc(m.description) + "\",\"progress\":" + std::to_string(m.progress) + ",\"target\":" + std::to_string(m.target) +
			 ",\"turnsLeft\":" + std::to_string(m.turnsLeft) + ",\"turns\":" + std::to_string(m.turns) + ",\"coins\":" +
			 std::to_string(m.rewardCoins) + "}";
	}
	return s + "]";
}

std::string tokensJson(const MatchState& s) {
	std::string out = "[";
	for (int p = 0; p < (int) s.players.size(); p++)
		for (int t = 0; t < TOKENS_PER_PLAYER; t++) out += ((p || t) ? "," : "") + std::to_string(s.players[p].progress[t]);
	return out + "]";
}

}  // namespace

int main(int argc, char** argv) {
	uint32_t seed = argc > 1 ? (uint32_t) std::strtoul(argv[1], nullptr, 10) : 7;
	std::string outPath = argc > 2 ? argv[2] : "trace.json";
	std::string contentDir = argc > 3 ? argv[3] : "Content/";

	MatchState s;
	s.players.resize(NUM_PLAYERS);
	for (int i = 0; i < NUM_PLAYERS; i++) {
		s.players[i].color = i;
		s.players[i].kind = i == 0 ? PlayerKind::Human : PlayerKind::Bot;
	}
	s.selfPlayer = 0;
	s.current = 0;
	s.phase = Phase::AwaitingRoll;

	RulesConfig rules;
	TurnMachine tm(s, rules);
	Rng rng(seed);

	MissionEngine engine;
	DirectorConfig dcfg;  // defaults, i.e. what the shipped config uses
	auto director = std::make_unique<DirectorStrategy>(dcfg, rules);
	DirectorStrategy* directorRef = director.get();  // the engine owns it; we only read its decision log
	engine.setStrategy(std::move(director));
	auto parsed = engine.loadFromJson(readFile(contentDir + "config/missions.json"));
	if (!parsed.errors.empty()) {
		for (auto& e : parsed.errors) std::fprintf(stderr, "mission error: %s\n", e.c_str());
		return 1;
	}
	engine.startMatch(0, seed);

	std::string frames;
	int frameCount = 0;
	int coins = 0;
	int completed = 0, failed = 0;

	auto emit = [&](const GameEvent& e, const std::vector<MissionUpdate>& updates) {
		std::string note;
		for (const auto& u : updates) {
			if (u.kind == MissionUpdate::Kind::Completed) {
				coins += u.instance.rewardCoins;
				completed++;
			} else if (u.kind == MissionUpdate::Kind::Failed) {
				failed++;
			}
			if (!note.empty()) note += " | ";
			note += std::string(missionUpdateKindName(u.kind)) + ":" + u.instance.id;
		}
		frames += (frameCount ? ",\n" : "");
		frames += "{\"i\":" + std::to_string(frameCount) + ",\"ev\":\"" + gameEventTypeName(e.type) + "\",\"player\":" +
				  std::to_string(e.player) + ",\"token\":" + std::to_string(e.token) + ",\"value\":" + std::to_string(e.value) +
				  ",\"steps\":" + std::to_string(e.steps) + ",\"victim\":" + std::to_string(e.victimPlayer) + ",\"turn\":" +
				  std::to_string(s.turnNumber) + ",\"tokens\":" + tokensJson(s) + ",\"coins\":" + std::to_string(coins) +
				  ",\"missions\":" + missionsJson(engine.activeInstances());
		if (!note.empty()) frames += ",\"note\":\"" + esc(note) + "\"";
		bool offered = false;
		for (const auto& u : updates) offered = offered || u.kind == MissionUpdate::Kind::Offered;
		if (offered && directorRef) {
			frames += ",\"decision\":\"" + esc(directorRef->lastDecisionLog()) + "\"";
			for (const auto& ev : directorRef->lastEvals()) {
				if (ev.runs > 0 && ev.id == updates[0].instance.id) {
					char pb[64];
					std::snprintf(pb, sizeof(pb), ",\"pickP\":%.4f,\"pickRuns\":%d", ev.p, ev.runs);
					frames += pb;
					break;
				}
			}
		}
		if (!updates.empty()) {
			frames += ",\"updates\":[";
			for (size_t i = 0; i < updates.size(); i++) {
				frames += (i ? "," : "");
				frames += "{\"kind\":\"" + std::string(missionUpdateKindName(updates[i].kind)) + "\",\"id\":\"" +
						  esc(updates[i].instance.id) + "\",\"title\":\"" + esc(updates[i].instance.title) + "\",\"desc\":\"" +
						  esc(updates[i].instance.description) + "\",\"coins\":" + std::to_string(updates[i].instance.rewardCoins) +
						  ",\"target\":" + std::to_string(updates[i].instance.target) + "}";
			}
			frames += "]";
		}
		frames += "}";
		frameCount++;
	};

	auto feed = [&](const std::vector<GameEvent>& evs) {
		for (const auto& e : evs) emit(e, engine.onEvent(e, s));
	};

	feed(tm.startMatch());
	for (int cmd = 0; cmd < 20000 && s.phase != Phase::MatchOver; cmd++) {
		if (s.phase == Phase::AwaitingRoll) {
			feed(tm.roll(rng.dice()));
		} else if (s.phase == Phase::AwaitingMove) {
			auto opts = rules::legalMoves(s, s.current);
			if (opts.empty()) break;
			MoveOption o = BotBrain::choose(s, opts, rng);
			feed(tm.move(o.token, o.value));
		} else {
			break;
		}
	}

	std::string ranking = "[";
	for (size_t i = 0; i < s.ranking.size(); i++) ranking += (i ? "," : "") + std::to_string(s.ranking[i]);
	ranking += "]";

	std::ofstream out(outPath);
	out << "{\"seed\":" << seed << ",\"layout\":" << layoutJson()
		<< ",\"names\":[\"You\",\"Bot Green\",\"Bot Yellow\",\"Bot Blue\"]"
		<< ",\"summary\":{\"frames\":" << frameCount << ",\"turns\":" << s.turnNumber << ",\"coins\":" << coins
		<< ",\"completed\":" << completed << ",\"failed\":" << failed << ",\"ranking\":" << ranking << "}"
		<< ",\"frames\":[\n" << frames << "\n]}\n";

	std::fprintf(stderr, "seed %u: %d frames, %d turns, %d missions completed, %d failed, %d coins -> %s\n", seed, frameCount,
				 s.turnNumber, completed, failed, coins, outPath.c_str());
	return 0;
}
