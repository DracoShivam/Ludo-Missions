#include "Views/Common/UiFactory.h"

#include "audio/AudioEngine.h"
#include "Views/Common/UiConfig.h"

namespace lm::ui {

static const ax::Rect PANEL_INSETS(30, 30, 40, 40);

ax::Label* makeLabel(const std::string& text, float size, bool bold, ax::Color3B color) {
	auto* l = ax::Label::createWithTTF(text, bold ? FONT_BLACK : FONT_BOLD, size);
	l->setColor(color);
	return l;
}

ax::ui::Button* makeButton(const std::string& text, ax::Size size, std::function<void()> onClick, ax::Color3B tint) {
	auto* b = ax::ui::Button::create(IMG_PANEL);
	b->setScale9Enabled(true);
	b->setCapInsets(PANEL_INSETS);
	b->setContentSize(size);
	b->setColor(tint);
	b->setTitleFontName(FONT_BLACK);
	b->setTitleFontSize(size.height * 0.45f);
	b->setTitleText(text);
	b->setTitleColor(ax::Color3B::WHITE);
	b->setPressedActionEnabled(true);
	b->addClickEventListener([onClick](ax::Object*) {
		if (onClick) onClick();
	});
	return b;
}

ax::ui::Scale9Sprite* makePanel(ax::Size size, ax::Color3B tint, uint8_t opacity) {
	auto* p = ax::ui::Scale9Sprite::create(IMG_PANEL);
	p->setCapInsets(PANEL_INSETS);
	p->setContentSize(size);
	p->setColor(tint);
	p->setOpacity(opacity);
	return p;
}

ax::Node* makeDieFace(int value, float px) {
	if (value >= 1 && value <= 6) {
		auto* s = ax::Sprite::create(diceFace(value));
		if (s) {
			s->setScale(px / 200.f);
			return s;
		}
	}
	// No dice7.png exists, and there should not be one: a boosted roll is a number, not a face.
	auto* n = ax::Node::create();
	auto* bg = makePanel(ax::Size(px, px), ax::Color3B(250, 246, 236), 255);
	n->addChild(bg);
	auto* label = makeLabel(std::to_string(value), px * 0.52f, true, ax::Color3B(40, 46, 70));
	n->addChild(label);
	return n;
}

void playSound(const char* path) {
	ax::AudioEngine::play2d(path, false, 0.8f);
}

}  // namespace lm::ui
