#pragma once

#include "axmol.h"

// All layout constants + asset paths in one place (design space 720x1280, origin bottom-left).
namespace lm::ui {

constexpr float DESIGN_W = 720.f;
constexpr float DESIGN_H = 1280.f;

// Board
constexpr float CELL = 46.f;
constexpr float BOARD_SIZE = CELL * 15.f;  // 690
constexpr float BOARD_CENTER_Y = 610.f;

// Top bar / mission HUD are anchored to the TOP of the visible area (offset from the top edge)
constexpr float TOP_BAR_FROM_TOP = 45.f;
constexpr float HUD_FROM_TOP = 165.f;
constexpr float HUD_CARD_W = 224.f;
constexpr float HUD_CARD_H = 120.f;
constexpr float HUD_CARD_X[3] = {124.f, 360.f, 596.f};

// Player panels
constexpr float PANEL_W = 330.f;
constexpr float PANEL_H = 80.f;
constexpr float PANEL_TOP_Y = 1005.f;
constexpr float PANEL_BOTTOM_Y = 215.f;
constexpr float PANEL_LEFT_X = 178.f;
constexpr float PANEL_RIGHT_X = 542.f;
constexpr float DICE_SIZE = 72.f;

// Power tray: below the human panel, clear of the board and of the DEV overlay.
constexpr float POWER_TRAY_Y = 132.f;
// The targeting banner sits over the lower edge of the board: unmissable while aiming, and
// absent the rest of the time, so it costs the resting layout nothing.
constexpr float POWER_BANNER_Y = 300.f;

// Colours (chaupar ViewUtils::getFlatColorByIndex)
inline ax::Color3B playerColor3B(int p) {
	static const ax::Color3B C[4] = {{234, 73, 55}, {34, 202, 87}, {240, 198, 31}, {30, 144, 255}};
	return C[p & 3];
}
inline ax::Color4B playerColor4B(int p, uint8_t a = 255) {
	auto c = playerColor3B(p);
	return ax::Color4B(c.r, c.g, c.b, a);
}
const ax::Color4B BG_COLOR(24, 32, 58, 255);
const ax::Color3B PANEL_DARK(40, 50, 84);
const ax::Color3B BUTTON_GREEN(46, 184, 92);

// Assets
constexpr const char* FONT_BOLD = "fonts/mikado_bold.ttf";
constexpr const char* FONT_BLACK = "fonts/mikado_black.ttf";
constexpr const char* FONT_TITLE = "fonts/luckiest_guy.ttf";
constexpr const char* IMG_PANEL = "images/ui/flat_round_white.png";
constexpr const char* IMG_COIN = "images/ui/coin.png";
constexpr const char* IMG_STAR = "images/board/star_outlined.png";
constexpr const char* IMG_TOKEN_BASE = "images/tokens/token_ludo_basic_base.png";
constexpr const char* IMG_TOKEN_COLOR = "images/tokens/token_ludo_basic_color.png";
constexpr const char* IMG_TOKEN_SHINE = "images/tokens/token_basic_shine.png";
constexpr const char* SND_DICE = "sounds/dice_roll.mp3";
constexpr const char* SND_MOVE = "sounds/token_move.mp3";
constexpr const char* SND_KILL = "sounds/kill_alter.mp3";
constexpr const char* SND_MY_TURN = "sounds/my_turn.mp3";
constexpr const char* SND_WIN = "sounds/celebrate_win.mp3";
constexpr const char* SND_COIN = "sounds/coin_collect.mp3";

inline std::string diceFace(int v) {
	return "images/dice/dice" + std::to_string(v) + ".png";
}
inline std::string diceRollFrame(int i) {
	return "images/dice/diceroll" + std::to_string(i) + ".png";
}

// Visible area helpers (FIXED_WIDTH: height varies by device)
inline ax::Vec2 visibleOrigin() {
	return ax::Director::getInstance()->getVisibleOrigin();
}
inline ax::Size visibleSize() {
	return ax::Director::getInstance()->getVisibleSize();
}
inline float topY(float fromTop) {
	auto safe = ax::Director::getInstance()->getSafeAreaRect();
	return safe.origin.y + safe.size.height - fromTop;
}

}  // namespace lm::ui
