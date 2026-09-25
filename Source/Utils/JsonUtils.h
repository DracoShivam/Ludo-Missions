#pragma once

#include <string>

#include "rapidjson/document.h"

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

}  // namespace json
}  // namespace lm
