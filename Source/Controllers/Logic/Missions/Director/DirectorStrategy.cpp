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
											   const OfferStats& stats, Rng& rng, bool mustOffer) {
	using Clock = std::chrono::steady_clock;
	auto t0 = Clock::now();
	auto elapsedMs = [&] { return std::chrono::duration<double, std::milli>(Clock::now() - t0).count(); };

	m_lastEvals.assign(candidates.size(), CandidateEval{});
	for (size_t i = 0; i < candidates.size(); i++) m_lastEvals[i].id = candidates[i]->def.id;

	// Stage 0: shortlist. Everything below is linear in the number of candidates, so a growing catalogue would
	// otherwise leave each one with too few samples to trust. Sampling is weighted rather than top-N so a
	// low-weight mission is unlikely rather than impossible, and it is seeded, so a decision stays reproducible.
	std::vector<size_t> eval;
	if (m_cfg.maxEvaluated <= 0 || (int) candidates.size() <= m_cfg.maxEvaluated) {
		for (size_t i = 0; i < candidates.size(); i++) eval.push_back(i);
	} else {
		std::vector<double> w(candidates.size());
		for (size_t i = 0; i < candidates.size(); i++) {
			const MissionDef& d = candidates[i]->def;
			auto it = stats.offersThisMatch.find(d.id);
			int offered = it == stats.offersThisMatch.end() ? 0 : it->second;
			w[i] = std::max(0.01, (d.weight / 10.0) / (1.0 + offered));
			if (d.id == stats.lastOfferedId) w[i] *= m_cfg.utility.repeatPenalty;
		}
		for (int picked = 0; picked < m_cfg.maxEvaluated; picked++) {
			double total = 0;
			for (double x : w) total += x;
			if (total <= 0) break;
			double r = rng.unit() * total;
			for (size_t i = 0; i < w.size(); i++) {
				if (w[i] <= 0) continue;
				r -= w[i];
				if (r <= 0) {
					eval.push_back(i);
					w[i] = 0;  // without replacement
					break;
				}
			}
		}
		std::sort(eval.begin(), eval.end());
	}

	// Stage 1: A* feasibility, under its own wall-clock cap. Without one, a large catalogue on an opening board
	// spends the whole decision budget here and leaves stage 2 with no samples at all -- every candidate then
	// scores P=0 and the offer is effectively arbitrary.
	for (size_t i : eval) {
		auto& ev = m_lastEvals[i];
		double left = m_cfg.astarBudgetMs > 0 ? m_cfg.astarBudgetMs - elapsedMs() : 0;
		if (m_cfg.astarBudgetMs > 0 && left <= 0.2) {
			ev.unknown = true;  // not searched; Unknown counts as feasible, so nothing is wrongly dropped
			continue;
		}
		SearchResult r = feasibilitySearch(*candidates[i], ctx.state, ctx.self, ctx.selfTurnIndex, m_rules, m_cfg.astarMaxExpansions,
										   candidates[i]->def.targetMin, left);
		ev.infeasible = r.verdict == SearchResult::Verdict::Infeasible;
		ev.unknown = r.verdict == SearchResult::Verdict::Unknown;
		ev.minTurns = r.minTurns;
	}

	// Stage 2: Monte Carlo, interleaved round-robin so the budget is shared fairly. The clock restarts here so
	// simBudgetMs means the simulation budget, as its name says, rather than whatever stage 1 left over.
	auto t1 = Clock::now();
	auto simElapsedMs = [&] { return std::chrono::duration<double, std::milli>(Clock::now() - t1).count(); };
	Rng sim(rng.next() | 1u);
	bool budgetHit = false;
	for (int round = 0; round < m_cfg.rollouts && !budgetHit; round++) {
		for (size_t i : eval) {
			auto& ev = m_lastEvals[i];
			if (ev.infeasible) continue;
			ev.runs++;
			const MissionDef& d = candidates[i]->def;
			if (d.targetMax > 0) {
				// One playout prices every target in the range at once.
				ev.achieved.push_back(simulateMissionAchieved(*candidates[i], ctx.state, ctx.self, ctx.selfTurnIndex, m_rules, sim, d.targetMax));
			} else if (simulateMissionOnce(*candidates[i], ctx.state, ctx.self, ctx.selfTurnIndex, m_rules, sim)) {
				ev.successes++;
			}
		}
		if (m_cfg.simBudgetMs > 0 && simElapsedMs() > m_cfg.simBudgetMs) budgetHit = true;
	}
	// Stage 3: utility + gate + softmax
	double center = m_difficulty.effectiveCenter(stats.humanRaceRank);
	std::vector<double> weights(candidates.size(), 0.0);
	double best = 0;
	for (size_t i = 0; i < candidates.size(); i++) {
		auto& ev = m_lastEvals[i];
		if (ev.infeasible || ev.runs == 0) continue;
		if (budgetHit && ev.runs < MIN_RUNS_UNDER_BUDGET) continue;
		const MissionDef& d = candidates[i]->def;
		if (d.targetMax > 0) {
			// Pick the target whose completion probability sits nearest the player's current difficulty centre.
			// Larger targets are strictly harder, so this is a scan over a monotone curve, not a search.
			int bestT = d.targetMin;
			int bestHits = 0;
			double bestDist = 1e9;
			for (int t = d.targetMin; t <= d.targetMax; t++) {
				int hits = 0;
				for (int a : ev.achieved) {
					if (a >= t) hits++;
				}
				double p = (double) hits / ev.runs;
				double dist = std::abs(p - center);
				if (dist < bestDist) {
					bestDist = dist;
					bestT = t;
					bestHits = hits;
				}
			}
			ev.solvedTarget = bestT;
			ev.successes = bestHits;
		}
		ev.p = (double) ev.successes / ev.runs;
		ev.utility = missionUtility(d, ev.p, ev.minTurns, center, m_cfg.difficulty.halfWidth, m_cfg.utility, stats);
		if (ev.p >= m_cfg.utility.minProbability) best = std::max(best, ev.utility);
	}
	std::optional<size_t> pick;
	if (best < m_cfg.utility.minUtility && mustOffer) {
		// Nothing clears the quality bar, but the player has no mission at all. Serve the least-bad feasible
		// candidate rather than nothing: an imperfect mission beats an empty HUD.
		// Rank by achievability first: serving something the player cannot do is worse than serving something
		// poorly fitted to their difficulty. Utility only breaks ties among equally achievable candidates.
		double bestP = -1.0, bestUtility = -1.0;
		for (size_t i = 0; i < candidates.size(); i++) {
			const auto& ev = m_lastEvals[i];
			if (ev.infeasible || ev.runs == 0) continue;
			bool better = ev.p > bestP + 1e-9 || (ev.p > bestP - 1e-9 && ev.utility > bestUtility);
			if (better) {
				bestP = ev.p;
				bestUtility = ev.utility;
				pick = i;
			}
		}
	} else if (best >= m_cfg.utility.minUtility) {
		double temp = std::max(1e-3, m_cfg.utility.temperature);
		double total = 0;
		for (size_t i = 0; i < candidates.size(); i++) {
			if (m_lastEvals[i].utility >= m_cfg.utility.minUtility &&
				m_lastEvals[i].p >= m_cfg.utility.minProbability) {  // weak or hopeless candidates never sampled
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
	int notShortlisted = 0;
	for (size_t i : order) {
		const auto& ev = m_lastEvals[i];
		if (ev.infeasible) {
			m_lastLog += " | " + ev.id + " INFEASIBLE";
		} else if (ev.runs == 0) {
			// Not shortlisted this time. Distinct from "scored zero", and worth keeping distinct in the log --
			// otherwise a mission that was never looked at reads like a mission that was judged hopeless.
			notShortlisted++;
		} else {
			std::snprintf(buf, sizeof(buf), " | %s P=%.2f minT=%d%s U=%.2f%s", ev.id.c_str(), ev.p, ev.minTurns, ev.unknown ? "?" : "", ev.utility,
						  (pick && *pick == i) ? " PICK" : "");
			m_lastLog += buf;
		}
	}
	if (notShortlisted > 0) {
		std::snprintf(buf, sizeof(buf), " | (%d not shortlisted)", notShortlisted);
		m_lastLog += buf;
	}
	if (!pick) m_lastLog += " | NO OFFER";
	return pick;
}

}  // namespace lm
