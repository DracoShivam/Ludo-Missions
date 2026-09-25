#pragma once

#include "Models/GameConfig.h"
#include "Views/Common/BaseView.h"

namespace lm {

class DiceView;

// Corner panel for one player: name, colour dot, dice, pending-roll chips, active-turn glow.
class PlayerPanelView : public BaseView {
public:
	static PlayerPanelView* create(int player, bool diceOnRight);
	bool initWith(int player, bool diceOnRight);

protected:
	void initListeners() override;

private:
	void setActive(bool on);
	void setChips(const std::vector<int>& rolls);
	int m_player = 0;
	ax::Node* m_bg = nullptr;
	ax::Node* m_glow = nullptr;
	ax::Label* m_name = nullptr;
	DiceView* m_dice = nullptr;
	ax::Node* m_chips = nullptr;
	TimingConfig m_timing;
};

}  // namespace lm
