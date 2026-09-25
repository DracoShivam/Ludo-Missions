#include "Controllers/Logic/Missions/MissionParser.h"

#include <set>

#include "Utils/JsonUtils.h"

namespace lm {

namespace {

bool toParamValue(const rapidjson::Value& v, ParamValue& out) {
	if (v.IsBool()) out = v.GetBool();
	else if (v.IsInt64()) out = (int64_t) v.GetInt64();
	else if (v.IsNumber()) out = v.GetDouble();
	else if (v.IsString()) out = std::string(v.GetString(), v.GetStringLength());
	else return false;
	return true;
}

// Generic JSON object -> Spec ("type" key becomes Spec::type).
Spec toSpec(const rapidjson::Value& obj) {
	Spec s;
	for (auto it = obj.MemberBegin(); it != obj.MemberEnd(); ++it) {
		std::string key(it->name.GetString(), it->name.GetStringLength());
		const auto& v = it->value;
		if (key == "type" && v.IsString()) {
			s.type = v.GetString();
		} else if (v.IsObject()) {
			s.params.children[key] = std::make_shared<Spec>(toSpec(v));
		} else if (v.IsArray()) {
			bool allObjects = v.Size() > 0;
			for (auto& e : v.GetArray()) allObjects &= e.IsObject();
			if (allObjects) {
				for (auto& e : v.GetArray()) s.params.lists[key].push_back(toSpec(e));
			} else {
				auto& arr = s.params.arrays[key];
				for (auto& e : v.GetArray()) {
					ParamValue pv;
					if (toParamValue(e, pv)) arr.push_back(pv);
				}
			}
		} else {
			ParamValue pv;
			if (toParamValue(v, pv)) s.params.values[key] = pv;
		}
	}
	return s;
}

const std::set<std::string> MISSION_KEYS = {"id", "enabled", "title", "description", "reward", "turns", "weight", "cooldownTurns",
											"maxPerMatch", "moments", "offerWhen", "objective", "_note"};

}  // namespace

MissionParseResult parseMissions(const std::string& json, const ConditionRegistry& conds, const ObjectiveRegistry& objs, int defaultCooldown) {
	MissionParseResult out;
	rapidjson::Document doc;
	std::string err;
	if (!json::parse(json, doc, err)) {
		out.errors.push_back("missions.json: " + err);
		return out;
	}
	auto mit = doc.IsObject() ? doc.FindMember("missions") : doc.MemberEnd();
	if (!doc.IsObject() || mit == doc.MemberEnd() || !mit->value.IsArray()) {
		out.errors.push_back("missions.json: root must be an object with a \"missions\" array");
		return out;
	}
	std::set<std::string> seen;
	int index = 0;
	for (auto& m : mit->value.GetArray()) {
		std::string where = "missions.json: missions[" + std::to_string(index++) + "]";
		if (!m.IsObject()) {
			out.errors.push_back(where + ": must be an object");
			continue;
		}
		MissionDef d;
		d.id = json::getString(m, "id", "");
		if (d.id.empty()) {
			out.errors.push_back(where + ": missing \"id\"");
			continue;
		}
		where = "missions.json: mission '" + d.id + "'";
		for (auto it = m.MemberBegin(); it != m.MemberEnd(); ++it) {
			if (!MISSION_KEYS.count(it->name.GetString())) out.warnings.push_back(where + ": unknown key '" + std::string(it->name.GetString()) + "' ignored");
		}
		if (seen.count(d.id)) {
			out.errors.push_back(where + ": duplicate id (second copy rejected)");
			continue;
		}
		seen.insert(d.id);
		d.enabled = json::getBool(m, "enabled", true);
		d.title = json::getString(m, "title", "");
		d.description = json::getString(m, "description", "");
		d.turns = json::getInt(m, "turns", 0);
		d.weight = json::getInt(m, "weight", 10);
		d.cooldownTurns = json::getInt(m, "cooldownTurns", defaultCooldown);
		d.maxPerMatch = json::getInt(m, "maxPerMatch", 0);
		const rapidjson::Value* reward = json::getObject(m, "reward");
		d.rewardCoins = reward ? json::getInt(*reward, "coins", -1) : -1;

		if (d.title.empty()) { out.errors.push_back(where + ": missing \"title\""); continue; }
		if (d.turns < 1) { out.errors.push_back(where + ": \"turns\" must be >= 1"); continue; }
		if (d.rewardCoins < 0) { out.errors.push_back(where + ": \"reward\": {\"coins\": N>=0} required"); continue; }
		if (d.weight <= 0) { out.errors.push_back(where + ": \"weight\" must be > 0"); continue; }

		auto mom = m.FindMember("moments");
		if (mom != m.MemberEnd()) {
			d.moments.clear();
			bool ok = mom->value.IsArray() && mom->value.Size() > 0;
			if (ok) {
				for (auto& v : mom->value.GetArray()) {
					std::string s = v.IsString() ? v.GetString() : "";
					if (s == "turnStart") d.moments.push_back(OfferMoment::TurnStart);
					else if (s == "afterRoll") d.moments.push_back(OfferMoment::AfterRoll);
					else ok = false;
				}
			}
			if (!ok) { out.errors.push_back(where + ": \"moments\" must be a non-empty array of \"turnStart\"|\"afterRoll\""); continue; }
		}

		const rapidjson::Value* offer = json::getObject(m, "offerWhen");
		d.offerWhen = offer ? toSpec(*offer) : Spec{"always", {}};
		const rapidjson::Value* obj = json::getObject(m, "objective");
		if (!obj) { out.errors.push_back(where + ": missing \"objective\""); continue; }
		d.objective = toSpec(*obj);

		auto cm = std::make_shared<CompiledMission>();
		std::string cerr;
		cm->offerWhen = conds.compile(d.offerWhen, where + ": offerWhen", cerr, &out.warnings);
		if (!cm->offerWhen) { out.errors.push_back(cerr); continue; }
		cm->makeObjective = objs.compile(d.objective, conds, where + ": objective", cerr, &out.warnings);
		if (!cm->makeObjective) { out.errors.push_back(cerr); continue; }
		cm->def = d;
		if (d.enabled) out.missions.push_back(cm);
	}
	return out;
}

}  // namespace lm
