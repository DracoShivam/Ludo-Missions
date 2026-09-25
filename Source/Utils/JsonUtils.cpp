#include "Utils/JsonUtils.h"

#include "rapidjson/error/en.h"

namespace lm {
namespace json {

bool parse(const std::string& text, rapidjson::Document& doc, std::string& err) {
	doc.Parse(text.c_str(), text.size());
	if (!doc.HasParseError()) {
		return true;
	}
	size_t offset = doc.GetErrorOffset();
	int line = 1;
	int col = 1;
	for (size_t i = 0; i < offset && i < text.size(); i++) {
		if (text[i] == '\n') {
			line++;
			col = 1;
		} else {
			col++;
		}
	}
	err = "line " + std::to_string(line) + ", col " + std::to_string(col) + ": " + rapidjson::GetParseError_En(doc.GetParseError());
	return false;
}

int getInt(const rapidjson::Value& obj, const char* key, int def) {
	if (!obj.IsObject()) {
		return def;
	}
	auto it = obj.FindMember(key);
	if (it == obj.MemberEnd() || !it->value.IsNumber()) {
		return def;
	}
	return it->value.IsInt() ? it->value.GetInt() : static_cast<int>(it->value.GetDouble());
}

double getDouble(const rapidjson::Value& obj, const char* key, double def) {
	if (!obj.IsObject()) {
		return def;
	}
	auto it = obj.FindMember(key);
	if (it == obj.MemberEnd() || !it->value.IsNumber()) {
		return def;
	}
	return it->value.GetDouble();
}

bool getBool(const rapidjson::Value& obj, const char* key, bool def) {
	if (!obj.IsObject()) {
		return def;
	}
	auto it = obj.FindMember(key);
	if (it == obj.MemberEnd() || !it->value.IsBool()) {
		return def;
	}
	return it->value.GetBool();
}

std::string getString(const rapidjson::Value& obj, const char* key, const std::string& def) {
	if (!obj.IsObject()) {
		return def;
	}
	auto it = obj.FindMember(key);
	if (it == obj.MemberEnd() || !it->value.IsString()) {
		return def;
	}
	return std::string(it->value.GetString(), it->value.GetStringLength());
}

const rapidjson::Value* getObject(const rapidjson::Value& obj, const char* key) {
	if (!obj.IsObject()) {
		return nullptr;
	}
	auto it = obj.FindMember(key);
	if (it == obj.MemberEnd() || !it->value.IsObject()) {
		return nullptr;
	}
	return &it->value;
}


namespace {
bool toParamValue(const rapidjson::Value& v, ParamValue& out) {
	if (v.IsBool()) out = v.GetBool();
	else if (v.IsInt64()) out = (int64_t) v.GetInt64();
	else if (v.IsNumber()) out = v.GetDouble();
	else if (v.IsString()) out = std::string(v.GetString(), v.GetStringLength());
	else return false;
	return true;
}
}  // namespace

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

}  // namespace json
}  // namespace lm
