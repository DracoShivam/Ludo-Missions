#pragma once

#include <vector>

#include "axmol.h"
#include "ui/CocosGUI.h"

#include "Events/PowerEvents.h"

namespace lm {

// The powers the player is holding, as tappable chips.
//
// Reads no controller. Everything it draws arrives in PowerState and every tap leaves as a Ui*
// event (AGENTS.md: Views never include Controllers/). That is not only a layering rule here --
// the previous tray asked a controller whether a power was usable, which is exactly the coupling
// that let the tray and the board disagree about what could be tapped.
class PowerTrayView : public ax::Node {
public:
	static PowerTrayView* create();
	bool init() override;
	void onEnter() override;
	void onExit() override;

private:
	void apply(const PowerState& st);
	void flashGranted(const std::string& id);
	void flashUsed(const PowerUsedMsg& u);
	void flashLine(const std::string& text, ax::Color3B tint);

	ax::Node* m_row = nullptr;
	ax::Label* m_hint = nullptr;
	PowerState m_state;
};

}  // namespace lm
