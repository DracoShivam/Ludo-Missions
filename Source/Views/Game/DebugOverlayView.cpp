#include "Views/Game/DebugOverlayView.h"

#include "Events/AppEvents.h"
#include "Events/DebugEvents.h"
#include "Events/EventBus.h"
#include "Views/Common/UiConfig.h"
#include "Views/Common/UiFactory.h"

namespace lm {

bool DebugOverlayView::init() {
	if (!Node::init()) {
		return false;
	}
	m_label = ui::makeLabel("", 16, false, ax::Color3B(180, 255, 180));
	m_label->setAnchorPoint(ax::Vec2(0, 0));
	addChild(m_label);

	auto* keys = ax::EventListenerKeyboard::create();
	keys->onKeyPressed = [](ax::EventKeyboard::KeyCode k, ax::Event*) {
		using K = ax::EventKeyboard::KeyCode;
		int code = (int) k;
		if (code >= (int) K::KEY_1 && code <= (int) K::KEY_6) {
			EventBus::publish(DebugForceNextRoll{code - (int) K::KEY_1 + 1});
		} else if (k == K::KEY_R) {
			EventBus::publish(DebugReloadConfig{});
		} else if (k == K::KEY_F) {
			EventBus::publish(DebugToggleFastBots{});
		} else if (k == K::KEY_C) {
			EventBus::publish(DebugResetCoins{});
		} else if (k == K::KEY_M) {
			EventBus::publish(DebugCycleForcedMission{});
		}
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(keys, this);
	refresh();
	return true;
}

void DebugOverlayView::initListeners() {
	EventBus::subscribe<DebugStateChanged>(this, [this](const DebugStateChanged& d) {
		if (d.hasForcedMission) {
			m_forcedMission = d.forcedMission;
		} else {
			m_forcedRoll = d.forcedRoll;
			m_fastBots = d.fastBots;
		}
		refresh();
	});
	EventBus::subscribe<MissionsReloaded>(this, [this](const MissionsReloaded& m) {
		m_missionsInfo = std::to_string(m.count) + " missions, " + std::to_string(m.errors) + " err";
		refresh();
	});
}

void DebugOverlayView::refresh() {
	std::string s = "DEV  1-6 roll | F fast | R reload | M mission | C coins\nnext roll: " +
					(m_forcedRoll ? std::to_string(m_forcedRoll) : std::string("-")) + "  fast bots: " + (m_fastBots ? "on" : "off") +
					"  force: " + (m_forcedMission.empty() ? "-" : m_forcedMission);
	if (!m_missionsInfo.empty()) s += "  (" + m_missionsInfo + ")";
	m_label->setString(s);
}

}  // namespace lm
