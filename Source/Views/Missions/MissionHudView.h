#pragma once

#include <map>

#include "Views/Common/BaseView.h"

namespace lm {

class MissionCardView;
class ToastView;

// Three stable card slots for active missions + toast banners + coin-fly to the coin counter.
// Listens ONLY to mission events: removing this view (or disabling missions) leaves the game untouched.
class MissionHudView : public BaseView {
public:
	static MissionHudView* create(ToastView* toasts, std::function<ax::Vec2()> coinTargetWorld);
	bool initWith(ToastView* toasts, std::function<ax::Vec2()> coinTargetWorld);

protected:
	void initListeners() override;

private:
	int freeSlot() const;
	void coinFly(ax::Vec2 fromWorld, int coins);
	std::map<int, MissionCardView*> m_cards;  // uid -> card
	std::map<int, int> m_slotOf;              // uid -> slot
	ToastView* m_toasts = nullptr;
	std::function<ax::Vec2()> m_coinTarget;
	ax::Label* m_empty = nullptr;
};

}  // namespace lm
