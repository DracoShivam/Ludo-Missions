#pragma once

#include "axmol.h"

namespace lm {

// One token: base + tinted colour cap (+ rotating shine when highlighted). Layering from chaupar ludo_board.prefab.
class TokenView : public ax::Node {
public:
	static TokenView* create(int player, int token);
	bool initWith(int player, int token);
	void setHighlighted(bool on);
	bool isHighlighted() const { return m_highlighted; }
	bool hitTest(const ax::Vec2& parentPoint) const;
	int player() const { return m_player; }
	int token() const { return m_token; }
	bool moving = false;

private:
	int m_player = 0;
	int m_token = 0;
	bool m_highlighted = false;
	ax::Sprite* m_shine = nullptr;
};

}  // namespace lm
