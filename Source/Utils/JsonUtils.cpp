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

}  // namespace json
}  // namespace lm
