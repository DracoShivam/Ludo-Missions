#pragma once

#include "axmol.h"
#include "ui/CocosGUI.h"

#include "Events/PowerEvents.h"

namespace lm {

// Shown only while a power is armed: what it is, what to tap, and a Cancel button.
//
// The Cancel button is the point. The previous build hid its only escape inside the tray chip and
// guarded it on the power still being "usable", so the instant your turn ended the way out
// vanished and the board stayed dead. This affordance depends on nothing but being armed.
class TargetBannerView : public ax::Node {
public:
	static TargetBannerView* create();
	bool init() override;
	void onEnter() override;
	void onExit() override;

private:
	void apply(const PowerState& st);
	ax::Node* m_panel = nullptr;
	ax::Label* m_title = nullptr;
	ax::Label* m_hint = nullptr;
};

}  // namespace lm
