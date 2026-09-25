#pragma once

#include "Views/Common/BaseView.h"

namespace lm {

// Coin icon + balance. Tweens on WalletChanged. Exposes the icon world position for coin-fly effects.
class CoinCounterView : public BaseView {
public:
	bool init() override;
	ax::Vec2 iconWorldPosition() const;

protected:
	void initListeners() override;

private:
	void setBalance(int balance, int delta);
	ax::Sprite* m_icon = nullptr;
	ax::Label* m_label = nullptr;
	int m_shown = 0;
};

}  // namespace lm
