#include "Controllers/Logic/Missions/Director/DirectorStrategy.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "Controllers/Logic/Missions/Director/FeasibilitySearch.h"
#include "Controllers/Logic/Missions/Director/RolloutSimulator.h"
#include "Controllers/Logic/Missions/Director/UtilityScorer.h"

namespace lm {

static constexpr int MIN_RUNS_UNDER_BUDGET = 16;

std::optional<size_t> DirectorStrategy::choose(const std::vector<CompiledMissionPtr>& candidates, const EvalContext& ctx, OfferMoment moment,
											   const OfferStats& stats, Rng& rng) {
	using Clock = std::chrono::steady_clock;
	auto t0 = Clock::now();
	auto elapsedMs = [&] { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); };

	m_lastEvals.assign(candidates.size(), CandidateEval{});
	// Stage 1: A* feasibility filter
	for (size_t i = 0; i < candidates.size(); i++) {
		auto& ev = m_lastEvals[i];
		ev.id = candidates[i]->def.id;
		SearchResult r = feasibilitySearch(*candidates[i], ctx.state, ctx.self, ctx.selfTurnIndex, m_rules, m_cfg.astarMaxExpansions);
		ev.infeasible = r.verdict == SearchResult::Verdict::Infeasible;
		ev.unknown = r.verdict == SearchResult::Verdict::Unknown;
		ev.minTurns = r.minTurns;
	}
	// Stage 2: Monte Carlo, interleaved round-robin so a time budget is shared fairly
	Rng sim(rng.next() | 1u);
	bool budgetHit = false;
	for (int round = 0; round < m_cfg.rollouts && !budgetHit; round++) {
		for (size_t i = 0; i < candidates.size(); i++) {
			auto& ev = m_lastEvals[i];
			if (ev.infeasible) continue;
			ev.runs++;
			if (simulateMissionOnce(*candidates[i], ctx.state, ctx.self, ctx.selfTurnIndex, m_rules, sim)) ev.successes++;
		}
		if (m_cfg.simBudgetMs > 0 && elapsedMs() > m_cfg.simBudgetMs) budgetHit = true;
	}
	// Stage 3: utility + gate + softmax
	double center = m_difficulty.effectiveCenter(stats.humanRaceRank);
	std::vector<double> weights(candidates.size(), 0.0);
	double best = 0;
	for (size_t i = 0; i < candidates.size(); i++) {
		auto& ev = m_lastEvals[i];
		if (ev.infeasible || ev.runs == 0) continue;
		if (budgetHit && ev.runs < MIN_RUNS_UNDER_BUDGET) continue;
		ev.p = (double) ev.successes / ev.runs;
		ev.utility = missionUtility(candidates[i]->def, ev.p, ev.minTurns, center, m_cfg.difficulty.halfWidth, m_cfg.utility, stats);
		best = std::max(best, ev.utility);
	}
	std::optional<size_t> pick;
	if (best >= m_cfg.utility.minUtility) {
		double temp = std::max(1e-3, m_cfg.utility.temperature);
		double total = 0;
		for (size_t i = 0; i < candidates.size(); i++) {
			if (m_lastEvals[i].utility > 0) {
				weights[i] = std::pow(m_lastEvals[i].utility / best, 1.0 / temp);  // normalised to avoid overflow
				total += weights[i];
			}
		}
		double r = rng.unit() * total;
		for (size_t i = 0; i < candidates.size() && total > 0; i++) {
			if (weights[i] <= 0) continue;
			pick = i;
			r -= weights[i];
			if (r <= 0) break;
		}
	}
	m_lastMs = elapsedMs();

	// Explainability log (designers tune with this)
	char buf[160];
	std::snprintf(buf, sizeof(buf), "%s center=%.2f %.1fms%s", moment == OfferMoment::TurnStart ? "turnStart" : "afterRoll", center, m_lastMs,
				  budgetHit ? " (budget)" : "");
	m_lastLog = buf;
	std::vector<size_t> order(candidates.size());
	for (size_t i = 0; i < order.size(); i++) order[i] = i;
	std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return m_lastEvals[a].utility > m_lastEvals[b].utility; });
	for (size_t i : order) {
		const auto& ev = m_lastEvals[i];
		if (ev.infeasible) {
			m_lastLog += " | " + ev.id + " INFEASIBLE";
		} else {
			std::snprintf(buf, sizeof(buf), " | %s P=%.2f minT=%d%s U=%.2f%s", ev.id.c_str(), ev.p, ev.minTurns, ev.unknown ? "?" : "", ev.utility,
						  (pick && *pick == i) ? " PICK" : "");
			m_lastLog += buf;
		}
	}
	if (!pick) m_lastLog += " | NO OFFER";
	return pick;
}

}  // namespace lm
