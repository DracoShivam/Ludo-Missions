#include "Controllers/Logic/Powers/PowerCatalog.h"

#include <algorithm>
#include <set>

#include "Utils/JsonUtils.h"

namespace lm {

namespace {
const char* TOP_KEYS[] = {"id", "title", "desc", "tier", "effect", "enabled", "weight", "_note"};
}

PowerParseResult parsePowers(const std::string& json, const EffectRegistry& effects) {
	PowerParseResult out;
	rapidjson::Document doc;
	std::string perr;
	if (!json::parse(json, doc, perr)) {
		out.errors.push_back("powers.json: " + perr);
		return out;
	}
	auto it = doc.FindMember("powers");
	if (it == doc.MemberEnd() || !it->value.IsArray()) {
		out.errors.push_back("powers.json: missing \"powers\" array");
		return out;
	}

	std::set<std::string> seen;
	int index = 0;
	for (auto& p : it->value.GetArray()) {
		std::string where = "powers.json: power #" + std::to_string(index++);
		if (!p.IsObject()) {
			out.errors.push_back(where + ": not an object");
			continue;
		}
		PowerDef d;
		d.id = json::getString(p, "id", "");
		if (d.id.empty()) {
			out.errors.push_back(where + ": missing \"id\"");
			continue;
		}
		where = "powers.json: power '" + d.id + "'";
		if (!seen.insert(d.id).second) {
			out.errors.push_back(where + ": duplicate id");
			continue;
		}

		for (auto m = p.MemberBegin(); m != p.MemberEnd(); ++m) {
			std::string key(m->name.GetString(), m->name.GetStringLength());
			if (std::find(std::begin(TOP_KEYS), std::end(TOP_KEYS), key) == std::end(TOP_KEYS)) {
				out.warnings.push_back(where + ": unknown key '" + key + "' ignored");
			}
		}

		d.title = json::getString(p, "title", "");
		d.desc = json::getString(p, "desc", "");
		d.enabled = json::getBool(p, "enabled", true);
		d.weight = json::getInt(p, "weight", 10);
		if (d.title.empty()) {
			out.errors.push_back(where + ": missing \"title\"");
			continue;
		}
		if (d.weight <= 0) {
			out.errors.push_back(where + ": \"weight\" must be > 0");
			continue;
		}
		std::string tier = json::getString(p, "tier", "");
		if (!powerTierFromString(tier, d.tier)) {
			out.errors.push_back(where + ": \"tier\" must be common, rare or epic");
			continue;
		}
		const rapidjson::Value* eff = json::getObject(p, "effect");
		if (!eff) {
			out.errors.push_back(where + ": missing \"effect\"");
			continue;
		}
		d.effect = json::toSpec(*eff);

		std::string cerr;
		PowerEffectPtr compiled = effects.compile(d.effect, where + ": effect", cerr, &out.warnings);
		if (!compiled) {
			out.errors.push_back(cerr);
			continue;
		}
		if (!d.enabled) {
			continue;  // parsed and validated, deliberately not served
		}

		auto cp = std::make_shared<CompiledPower>();
		cp->def = std::move(d);
		cp->effect = std::move(compiled);
		out.powers.push_back(cp);
	}
	return out;
}

PowerCatalog::PowerCatalog() {
	registerBuiltinEffects(m_effects);
}

PowerParseResult PowerCatalog::loadFromJson(const std::string& json) {
	PowerParseResult r = parsePowers(json, m_effects);
	if (!r.powers.empty() || r.errors.empty()) {
		m_powers = r.powers;  // a wholly broken file leaves the previous catalogue in place
	}
	return r;
}

const CompiledPower* PowerCatalog::find(const std::string& id) const {
	for (const auto& p : m_powers) {
		if (p->def.id == id) return p.get();
	}
	return nullptr;
}

const CompiledPower* PowerCatalog::draw(PowerTier tier, Rng& rng) const {
	for (int t = (int) tier; t >= 0; t--) {
		std::vector<const CompiledPower*> pool;
		int total = 0;
		for (const auto& p : m_powers) {
			if ((int) p->def.tier != t) continue;
			pool.push_back(p.get());
			total += p->def.weight;
		}
		if (pool.empty()) continue;
		int r = rng.range(1, total);
		for (const auto* p : pool) {
			r -= p->def.weight;
			if (r <= 0) return p;
		}
		return pool.back();
	}
	return nullptr;
}

std::vector<std::string> PowerCatalog::ids() const {
	std::vector<std::string> out;
	for (const auto& p : m_powers) out.push_back(p->def.id);
	return out;
}

}  // namespace lm
