#pragma once

#include <functional>
#include <string>

#include "axmol.h"
#include "ui/CocosGUI.h"

namespace lm::ui {

ax::Label* makeLabel(const std::string& text, float size, bool bold = true, ax::Color3B color = ax::Color3B::WHITE);
ax::ui::Button* makeButton(const std::string& text, ax::Size size, std::function<void()> onClick, ax::Color3B tint = ax::Color3B(46, 184, 92));
ax::ui::Scale9Sprite* makePanel(ax::Size size, ax::Color3B tint, uint8_t opacity = 255);
void playSound(const char* path);

}  // namespace lm::ui
