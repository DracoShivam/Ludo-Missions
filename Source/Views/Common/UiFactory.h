#pragma once

#include <functional>
#include <string>

#include "axmol.h"
#include "ui/CocosGUI.h"

namespace lm::ui {

ax::Label* makeLabel(const std::string& text, float size, bool bold = true, ax::Color3B color = ax::Color3B::WHITE);
ax::ui::Button* makeButton(const std::string& text, ax::Size size, std::function<void()> onClick, ax::Color3B tint = ax::Color3B(46, 184, 92));
ax::ui::Scale9Sprite* makePanel(ax::Size size, ax::Color3B tint, uint8_t opacity = 255);

// A die face sized to `px`. Values 1..6 use the artwork; a boosted roll has no face to draw, so it
// falls back to a numbered chip rather than requesting an image that does not exist.
ax::Node* makeDieFace(int value, float px);
void playSound(const char* path);

}  // namespace lm::ui
