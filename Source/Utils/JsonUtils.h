#pragma once

#include <string>

#include "rapidjson/document.h"

#include "Models/Params.h"

namespace lm {
namespace json {

// Parses `text` into `doc`. On failure fills `err` with "line L, col C: <reason>" and returns false.
bool parse(const std::string& text, rapidjson::Document& doc, std::string& err);

int getInt(const rapidjson::Value& obj, const char* key, int def);
double getDouble(const rapidjson::Value& obj, const char* key, double def);
bool getBool(const rapidjson::Value& obj, const char* key, bool def);
std::string getString(const rapidjson::Value& obj, const char* key, const std::string& def);
// Returns the member object or nullptr if missing / not an object.
const rapidjson::Value* getObject(const rapidjson::Value& obj, const char* key);

// Generic JSON object -> Spec (the "type" key becomes Spec::type). Shared by the mission and
// power parsers: both compile "{type, ...params}" blocks through their own registry.
Spec toSpec(const rapidjson::Value& obj);

}  // namespace json
}  // namespace lm
