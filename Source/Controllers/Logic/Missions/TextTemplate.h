#pragma once

#include <map>
#include <string>

namespace lm {

// Replaces {name} with vars[name]. Unknown placeholders are left as-is.
std::string renderTemplate(const std::string& tpl, const std::map<std::string, int>& vars);

}  // namespace lm
