#include "Views/Common/CoinCounterView.h"

#include "Events/EventBus.h"
#include "Events/WalletEvents.h"
#include "Views/Common/UiConfig.h"
#include "Views/Common/UiFactory.h"

namespace lm {

bool CoinCounterView::init() {
	if (!Node::init()) {
		return false;
	}
	auto* bg = ui::makePanel(ax::Size(170, 56), ui::PANEL_DARK, 230);
	addChild(bg);
	m_icon = ax::Sprite::create(ui::IMG_COIN);
	m_icon->setScale(44.f / m_icon->getContentSize().width);
	m_icon->setPosition(-58, 0);
	addChild(m_icon);
	m_label = ui::makeLabel("0", 30);
	m_label->setAnchorPoint(ax::Vec2(0, 0.5f));
	m_label->setPosition(-28, 0);
	addChild(m_label);
	return true;
}

void CoinCounterView::initListeners() {
	EventBus::subscribe<WalletChanged>(this, [this](const WalletChanged& e) { setBalance(e.balance, e.delta); });
}

void CoinCounterView::setBalance(int balance, int delta) {
	m_label->stopAllActions();
	int from = m_shown;
	m_shown = balance;
	if (delta <= 0 || from == balance) {
		m_label->setString(std::to_string(balance));
		return;
	}
	auto* label = m_label;
	label->runAction(ax::ActionFloat::create(0.6f, (float) from, (float) balance, [label](float v) { label->setString(std::to_string((int) v)); }));
	m_icon->stopAllActions();
	float s = m_icon->getScale();
	m_icon->runAction(ax::Sequence::create(ax::ScaleTo::create(0.12f, s * 1.3f), ax::ScaleTo::create(0.12f, s), nullptr));
}

ax::Vec2 CoinCounterView::iconWorldPosition() const {
	return m_icon->getParent()->convertToWorldSpace(m_icon->getPosition());
}

}  // namespace lm
