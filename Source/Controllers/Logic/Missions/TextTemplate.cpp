#include "Controllers/Logic/Missions/TextTemplate.h"

namespace lm {

std::string renderTemplate(const std::string& tpl, const std::map<std::string, int>& vars) {
	std::string out;
	out.reserve(tpl.size());
	for (size_t i = 0; i < tpl.size(); i++) {
		if (tpl[i] == '{') {
			size_t end = tpl.find('}', i);
			if (end != std::string::npos) {
				auto it = vars.find(tpl.substr(i + 1, end - i - 1));
				if (it != vars.end()) {
					out += std::to_string(it->second);
					i = end;
					continue;
				}
			}
		}
		out += tpl[i];
	}
	return out;
}

}  // namespace lm
