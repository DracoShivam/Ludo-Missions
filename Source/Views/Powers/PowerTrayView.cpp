#include "Views/Powers/PowerTrayView.h"

#include "Events/EventBus.h"
#include "Events/GameEvents.h"
#include "Events/UiEvents.h"
#include "Views/Common/PowerLook.h"
#include "Views/Common/UiConfig.h"
#include "Views/Common/UiFactory.h"

namespace lm {

namespace {
constexpr float BTN_W = 150.f, BTN_H = 54.f, GAP = 12.f;
}

PowerTrayView* PowerTrayView::create() {
	auto* v = new (std::nothrow) PowerTrayView();
	if (v && v->init()) {
		v->autorelease();
		return v;
	}
	delete v;
	return nullptr;
}

bool PowerTrayView::init() {
	if (!Node::init()) {
		return false;
	}
	m_row = ax::Node::create();
	addChild(m_row);

	m_hint = ui::makeLabel("Finish missions to earn powers", 15, false, ax::Color3B(148, 156, 184));
	m_hint->setPosition(0, -36);
	addChild(m_hint);
	return true;
}

void PowerTrayView::onEnter() {
	Node::onEnter();
	EventBus::subscribe<PowerState>(this, [this](const PowerState& st) { apply(st); });
	EventBus::subscribe<PowerGranted>(this, [this](const PowerGranted& g) { flashGranted(g.title); });
	EventBus::subscribe<PowerUsedMsg>(this, [this](const PowerUsedMsg& u) { flashUsed(u); });
	// Two power outcomes happen on a later turn, long after the chip was tapped. Unannounced, a
	// shield simply stops working and a skipped rival looks like a dropped turn.
	EventBus::subscribe<GameEventMsg>(this, [this](const GameEventMsg& m) {
		if (m.event.type == GameEventType::SHIELD_EXPIRED) {
			flashLine("Shield has worn off", ax::Color3B(126, 154, 226));
		} else if (m.event.type == GameEventType::TURN_SKIPPED) {
			flashLine("A rival's turn was skipped", ax::Color3B(214, 104, 66));
		}
	});
}

void PowerTrayView::onExit() {
	EventBus::unsubscribeAll(this);
	Node::onExit();
}

void PowerTrayView::apply(const PowerState& st) {
	m_state = st;
	m_row->removeAllChildren();

	if (st.chips.empty()) {
		m_hint->setString("Finish missions to earn powers");
		m_hint->setColor(ax::Color3B(148, 156, 184));
		return;
	}

	float total = st.chips.size() * BTN_W + (st.chips.size() - 1) * GAP;
	float x = -total / 2 + BTN_W / 2;

	for (const auto& chip : st.chips) {
		bool armed = chip.id == st.armedId;
		bool live = chip.usable || armed;
		std::string label = chip.title;
		ax::Color3B tint = live ? ui::powerTierColor((int) chip.tier) : ui::powerTierColorDim((int) chip.tier);

		// An armed chip gets a pulsing halo behind it. A 7% scale bump, which is all it had before,
		// is easy to miss on a busy board -- and the player needs to know at a glance which power is
		// waiting on a target, because that is the state every tap is about to be interpreted in.
		if (armed) {
			auto* halo = ui::makePanel(ax::Size(BTN_W + 22, BTN_H + 22), ui::powerTierColor((int) chip.tier), 120);
			halo->setPosition(ax::Vec2(x, 0));
			m_row->addChild(halo, -1);
			halo->runAction(ax::RepeatForever::create(
				ax::Sequence::create(ax::FadeTo::create(0.5f, 40), ax::FadeTo::create(0.5f, 130), nullptr)));
		}

		auto* btn = ui::makeButton(label, ax::Size(BTN_W, BTN_H),
								   [id = chip.id] {
									   UiPowerTapped e;
									   e.id = id;
									   EventBus::publish(e);  // armed or not, the controller decides
								   },
								   tint);
		btn->setPosition(ax::Vec2(x, 0));
		// Held but not spendable reads as a darker version of its own colour, never as a faded one:
		// fading toward the dark panel behind turns every tier into the same grey.
		btn->setOpacity(live ? 255 : 235);
		btn->setTitleColor(live ? ax::Color3B::WHITE : ax::Color3B(164, 172, 196));
		m_row->addChild(btn);

		if (armed) {
			btn->runAction(ax::RepeatForever::create(
				ax::Sequence::create(ax::ScaleTo::create(0.5f, 1.08f), ax::ScaleTo::create(0.5f, 1.0f), nullptr)));
		}

		// A count badge, so "x2" is not buried inside the title on a narrow chip.
		if (chip.count > 1) {
			auto* badge = ui::makeLabel("x" + std::to_string(chip.count), 13, true, ax::Color3B(255, 240, 200));
			badge->setPosition(x + BTN_W / 2 - 13, BTN_H / 2 - 11);
			badge->setOpacity(live ? 255 : 150);
			m_row->addChild(badge, 1);
		}
		x += BTN_W + GAP;
	}

	if (!st.armedId.empty()) {
		m_hint->setString("");  // the banner is speaking now
	} else if (!st.yourTurn) {
		m_hint->setString("Use a power on your turn");
		m_hint->setColor(ax::Color3B(148, 156, 184));
	} else {
		bool any = false;
		for (const auto& c : st.chips) any = any || c.usable;
		m_hint->setString(any ? "" : "No power can be used right now");
		m_hint->setColor(ax::Color3B(148, 156, 184));
	}
}

void PowerTrayView::flashLine(const std::string& text, ax::Color3B tint) {
	m_hint->stopAllActions();
	m_hint->setString(text);
	m_hint->setColor(tint);
	m_hint->setScale(0.85f);
	m_hint->runAction(ax::Sequence::create(
		ax::EaseBackOut::create(ax::ScaleTo::create(0.2f, 1.f)), ax::DelayTime::create(2.4f),
		ax::CallFunc::create([this] { apply(m_state); }), nullptr));
}

void PowerTrayView::flashUsed(const PowerUsedMsg& u) {
	flashLine(u.title + ": " + u.text, ui::powerTierColor((int) u.tier));
}

void PowerTrayView::flashGranted(const std::string& title) {
	flashLine("Power earned: " + title, ax::Color3B(255, 214, 96));
}

}  // namespace lm
