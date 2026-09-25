#include "Views/Powers/TargetBannerView.h"

#include "Events/EventBus.h"
#include "Events/UiEvents.h"
#include "Views/Common/PowerLook.h"
#include "Views/Common/UiConfig.h"
#include "Views/Common/UiFactory.h"

namespace lm {

namespace {
constexpr float W = 560.f, H = 96.f;
}

TargetBannerView* TargetBannerView::create() {
	auto* v = new (std::nothrow) TargetBannerView();
	if (v && v->init()) {
		v->autorelease();
		return v;
	}
	delete v;
	return nullptr;
}

bool TargetBannerView::init() {
	if (!Node::init()) {
		return false;
	}
	m_panel = ax::Node::create();
	addChild(m_panel);

	auto* bg = ui::makePanel(ax::Size(W, H), ax::Color3B(24, 30, 54), 242);
	m_panel->addChild(bg);

	m_title = ui::makeLabel("", 20, true, ax::Color3B::WHITE);
	m_title->setPosition(-W / 2 + 22, H / 2 - 26);
	m_title->setAnchorPoint(ax::Vec2(0, 0.5f));
	m_panel->addChild(m_title);

	m_hint = ui::makeLabel("", 15, false, ax::Color3B(206, 214, 238));
	m_hint->setPosition(-W / 2 + 22, -6);
	m_hint->setAnchorPoint(ax::Vec2(0, 0.5f));
	m_panel->addChild(m_hint);

	auto* cancel = ui::makeButton("Cancel", ax::Size(132, 46), [] { EventBus::publish(UiPowerCancelled{}); },
								  ax::Color3B(92, 100, 128));
	cancel->setPosition(ax::Vec2(W / 2 - 84, -H / 2 + 30));
	m_panel->addChild(cancel);

	setVisible(false);  // at rest the banner costs the layout nothing
	return true;
}

void TargetBannerView::onEnter() {
	Node::onEnter();
	EventBus::subscribe<PowerState>(this, [this](const PowerState& st) { apply(st); });
}

void TargetBannerView::onExit() {
	EventBus::unsubscribeAll(this);
	Node::onExit();
}

void TargetBannerView::apply(const PowerState& st) {
	if (st.armedId.empty()) {
		setVisible(false);
		return;
	}
	std::string title = st.armedId;
	int tier = 0;
	for (const auto& c : st.chips) {
		if (c.id == st.armedId) {
			title = c.title;
			tier = (int) c.tier;
		}
	}
	m_title->setString(title);
	m_title->setColor(ui::powerTierColor(tier));
	m_hint->setString(st.armedHint.empty() ? "Tap a highlighted token" : st.armedHint);

	if (!isVisible()) {
		setVisible(true);
		m_panel->setScale(0.92f);
		m_panel->runAction(ax::EaseBackOut::create(ax::ScaleTo::create(0.18f, 1.f)));
	}
}

}  // namespace lm
