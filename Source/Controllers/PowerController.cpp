#include "Controllers/PowerController.h"

#include "axmol.h"

#include "Controllers/ConfigController.h"
#include "Events/EventBus.h"
#include "Events/DebugEvents.h"
#include "Events/PowerEvents.h"
#include "Utils/Log.h"

namespace lm {

PowerController* PowerController::sharedController() {
	static PowerController inst;
	return &inst;
}

void PowerController::init() {
	if (!m_subscribed) {
		// The DEV reload key rebuilds the config and the missions; without this it silently skipped
		// powers.json, so editing a power and pressing R appeared to do nothing.
		EventBus::subscribe<DebugReloadConfig>(this, [this](const DebugReloadConfig&) { reloadCatalog(); });
		m_subscribed = true;
	}
	reloadCatalog();
}

void PowerController::reloadCatalog() {
	const GameConfig& cfg = ConfigController::sharedController()->config();
	m_cfg = cfg.powers;
	m_rng.seed(cfg.debug.rngSeed ? cfg.debug.rngSeed + 3 : 0);  // own stream

	std::string json = ax::FileUtils::getInstance()->getStringFromFile(m_cfg.file);
	auto r = m_catalog.loadFromJson(json);
	for (const auto& e : r.errors) LM_LOG_ERROR("%s", e.c_str());
	for (const auto& w : r.warnings) LM_LOG("%s", w.c_str());
	LM_LOG("powers loaded: %d (%d errors, %d warnings)", (int) r.powers.size(), (int) r.errors.size(), (int) r.warnings.size());
	// Titles, colours and tiers may have changed under the player's feet; redraw the tray.
	EventBus::publish(PowerInventoryChanged{});
}

void PowerController::resetMatch() {
	m_held.clear();
	EventBus::publish(PowerInventoryChanged{});
}

PowerTier PowerController::tierForProbability(double p) const {
	if (p < 0) return PowerTier::Common;  // unmeasured: never pay the top tier by accident
	if (p >= m_cfg.commonAbove) return PowerTier::Common;
	if (p >= m_cfg.rareAbove) return PowerTier::Rare;
	return PowerTier::Epic;
}

bool PowerController::grant(int seat, const std::string& id, const std::string& reason) {
	if (!m_cfg.enabled || id.empty()) {
		return false;
	}
	const CompiledPower* p = m_catalog.find(id);
	if (!p) {
		LM_LOG_ERROR("power '%s' is not in the catalogue (%s)", id.c_str(), reason.c_str());
		return false;
	}
	auto& seatHeld = m_held[seat];
	if (total(seat) >= m_cfg.maxTotalHeld || seatHeld[id] >= m_cfg.maxHeld) {
		// Dropped rather than banked. A stockpile is the snowball this feature has to avoid, and
		// quietly queueing grants would hide that from the player.
		LM_LOG("power %s dropped for seat %d (%s): at the cap", id.c_str(), seat, reason.c_str());
		return false;
	}
	seatHeld[id]++;
	LM_LOG("power granted: %s to seat %d (%s), now %d", id.c_str(), seat, reason.c_str(), seatHeld[id]);

	PowerGranted g;
	g.seat = seat;
	g.id = id;
	g.title = p->def.title;
	g.tier = p->def.tier;
	g.reason = reason;
	EventBus::publish(g);
	EventBus::publish(PowerInventoryChanged{});
	return true;
}

std::string PowerController::drawId(PowerTier tier) {
	const CompiledPower* p = m_catalog.draw(tier, m_rng);
	return p ? p->def.id : std::string();
}

bool PowerController::grantTier(int seat, PowerTier tier, const std::string& reason) {
	const CompiledPower* p = m_catalog.draw(tier, m_rng);
	return p ? grant(seat, p->def.id, reason) : false;
}

bool PowerController::consume(int seat, const std::string& id) {
	auto s = m_held.find(seat);
	if (s == m_held.end()) {
		return false;
	}
	auto it = s->second.find(id);
	if (it == s->second.end() || it->second <= 0) {
		return false;
	}
	it->second--;
	EventBus::publish(PowerInventoryChanged{});
	return true;
}

int PowerController::count(int seat, const std::string& id) const {
	auto s = m_held.find(seat);
	if (s == m_held.end()) {
		return 0;
	}
	auto it = s->second.find(id);
	return it == s->second.end() ? 0 : it->second;
}

int PowerController::total(int seat) const {
	auto s = m_held.find(seat);
	if (s == m_held.end()) {
		return 0;
	}
	int n = 0;
	for (const auto& [_, c] : s->second) n += c;
	return n;
}

std::vector<std::string> PowerController::held(int seat) const {
	std::vector<std::string> out;
	for (const auto& p : m_catalog.all()) {
		if (count(seat, p->def.id) > 0) out.push_back(p->def.id);
	}
	return out;
}

}  // namespace lm
