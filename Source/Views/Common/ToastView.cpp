#include "Views/Common/ToastView.h"

#include "Views/Common/UiFactory.h"

namespace lm {

bool ToastView::init() {
	return Node::init();
}

void ToastView::show(const std::string& text, ax::Color3B tint) {
	m_queue.push_back({text, tint});
	if (!m_busy) next();
}

void ToastView::next() {
	if (m_queue.empty()) {
		m_busy = false;
		return;
	}
	m_busy = true;
	Item it = m_queue.front();
	m_queue.pop_front();
	auto* node = ax::Node::create();
	auto* label = ui::makeLabel(it.text, 30);
	auto* bg = ui::makePanel(ax::Size(label->getContentSize().width + 60, 70), it.tint, 240);
	node->addChild(bg);
	node->addChild(label);
	node->setScale(0.3f);
	node->setOpacity(0);
	node->setCascadeOpacityEnabled(true);
	addChild(node);
	node->runAction(ax::Sequence::create(ax::Spawn::create(ax::EaseBackOut::create(ax::ScaleTo::create(0.2f, 1.f)), ax::FadeIn::create(0.15f), nullptr),
										 ax::DelayTime::create(1.2f), ax::Spawn::create(ax::MoveBy::create(0.2f, ax::Vec2(0, 30)), ax::FadeOut::create(0.2f), nullptr),
										 ax::CallFunc::create([this] { next(); }), ax::RemoveSelf::create(), nullptr));
}

}  // namespace lm
