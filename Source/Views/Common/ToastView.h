#pragma once

#include <deque>
#include <string>

#include "axmol.h"

namespace lm {

// Queued banner messages (never overlap). show() enqueues; each lasts ~1.6s.
class ToastView : public ax::Node {
public:
	bool init() override;
	void show(const std::string& text, ax::Color3B tint);

private:
	void next();
	struct Item {
		std::string text;
		ax::Color3B tint;
	};
	std::deque<Item> m_queue;
	bool m_busy = false;
};

}  // namespace lm
