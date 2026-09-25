#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace lm {

using ParamValue = std::variant<int64_t, double, bool, std::string>;

struct Spec;

// Generic parsed JSON object (pure data). Scalars -> values, objects -> children, arrays of objects -> lists,
// arrays of scalars -> arrays.
struct Params {
	std::map<std::string, ParamValue> values;
	std::map<std::string, std::shared_ptr<Spec>> children;
	std::map<std::string, std::vector<Spec>> lists;
	std::map<std::string, std::vector<ParamValue>> arrays;

	bool has(const std::string& k) const { return values.count(k) || children.count(k) || lists.count(k) || arrays.count(k); }
	int getInt(const std::string& k, int def) const {
		auto it = values.find(k);
		if (it == values.end()) return def;
		if (auto* i = std::get_if<int64_t>(&it->second)) return (int) *i;
		if (auto* d = std::get_if<double>(&it->second)) return (int) *d;
		return def;
	}
	bool getBool(const std::string& k, bool def) const {
		auto it = values.find(k);
		if (it == values.end()) return def;
		if (auto* b = std::get_if<bool>(&it->second)) return *b;
		return def;
	}
	std::string getString(const std::string& k, const std::string& def) const {
		auto it = values.find(k);
		if (it == values.end()) return def;
		if (auto* s = std::get_if<std::string>(&it->second)) return *s;
		return def;
	}
	std::vector<std::string> keys() const {
		std::vector<std::string> out;
		for (auto& [k, _] : values) out.push_back(k);
		for (auto& [k, _] : children) out.push_back(k);
		for (auto& [k, _] : lists) out.push_back(k);
		for (auto& [k, _] : arrays) out.push_back(k);
		return out;
	}
};

// A parsed but uncompiled building block: {"type": "...", ...params}
struct Spec {
	std::string type;
	Params params;
};

}  // namespace lm
