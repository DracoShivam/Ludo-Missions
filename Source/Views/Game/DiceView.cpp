#include "Views/Game/DiceView.h"

#include "Events/EventBus.h"
#include "Events/UiEvents.h"
#include "Views/Common/UiConfig.h"
#include "Views/Common/UiFactory.h"

namespace lm {

bool DiceView::init() {
	if (!Node::init()) {
		return false;
	}
	m_sprite = ax::Sprite::create(ui::diceFace(6));
	m_sprite->setScale(ui::DICE_SIZE / 200.f);
	addChild(m_sprite);

	auto* touch = ax::EventListenerTouchOneByOne::create();
	touch->setSwallowTouches(true);
	touch->onTouchBegan = [this](ax::Touch* t, ax::Event*) {
		if (!m_tappable || !isVisible()) return false;
		ax::Vec2 p = convertToNodeSpace(t->getLocation());
		float h = ui::DICE_SIZE * 0.7f;
		return std::abs(p.x) < h && std::abs(p.y) < h;
	};
	touch->onTouchEnded = [this](ax::Touch*, ax::Event*) {
		if (!m_tappable) return;
		setTappable(false);
		EventBus::publish(UiRollDiceTapped{});
	};
	_eventDispatcher->addEventListenerWithSceneGraphPriority(touch, this);
	return true;
}

void DiceView::setFace(int value) {
	m_sprite->setTexture(ui::diceFace(value));
}

void DiceView::setTappable(bool on) {
	m_tappable = on;
	float base = ui::DICE_SIZE / 200.f;
	m_sprite->stopActionByTag(7);
	m_sprite->setScale(base);
	if (on) {
		auto* pulse = ax::RepeatForever::create(
			ax::Sequence::create(ax::ScaleTo::create(0.35f, base * 1.15f), ax::ScaleTo::create(0.35f, base), nullptr));
		pulse->setTag(7);
		m_sprite->runAction(pulse);
	}
}

void DiceView::roll(int value, float duration) {
	setTappable(false);
	m_sprite->stopAllActions();
	float base = ui::DICE_SIZE / 200.f;
	int frames = std::max(1, (int) (duration / 0.035f));
	ax::Vector<ax::FiniteTimeAction*> seq;
	for (int i = 0; i < frames; i++) {
		int f = (i % 6) + 1;
		seq.pushBack(ax::CallFunc::create([this, f] { m_sprite->setTexture(ui::diceRollFrame(f)); }));
		seq.pushBack(ax::DelayTime::create(duration / frames));
	}
	seq.pushBack(ax::CallFunc::create([this, value] { setFace(value); }));
	m_sprite->runAction(ax::Sequence::create(seq));
	m_sprite->runAction(ax::Sequence::create(ax::ScaleTo::create(duration * 0.4f, base * 1.3f),
											 ax::ScaleTo::create(duration * 0.6f, base), nullptr));
	ui::playSound(ui::SND_DICE);
}

}  // namespace lm
