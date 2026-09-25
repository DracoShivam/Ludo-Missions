#pragma once

#include <array>

#include "Models/GameConfig.h"
#include "Models/GameEvent.h"
#include "Views/Common/BaseView.h"

namespace lm {

class TokenView;

// Draws the board, owns the 16 TokenViews, animates moves/captures, highlights the human's movable tokens,
// shows roll-choice chips and publishes UiTokenTapped.
class BoardView : public BaseView {
public:
	bool init() override;

protected:
	void initListeners() override;

private:
	void drawBoard();
	void placeAll();
	void relayout();
	void animateMove(const GameEvent& e, float animScale);
	void animateCapture(const GameEvent& e, float animScale);
	void clearHighlights();
	void closeChoice();
	TokenView* tokenAt(const ax::Vec2& localPoint) const;
	ax::Vec2 basePosition(int player, int token) const;

	std::array<std::array<int, 4>, 4> m_progress{};
	std::array<std::array<TokenView*, 4>, 4> m_tokens{};
	ax::Node* m_tokenLayer = nullptr;
	ax::Node* m_choice = nullptr;
	TimingConfig m_timing;
	int m_self = 0;
};

}  // namespace lm
