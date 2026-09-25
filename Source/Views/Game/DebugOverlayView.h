#pragma once

#include "Views/Common/BaseView.h"

namespace lm {

// DEV only: keyboard shortcuts (1-6 force roll, R reload, F fast bots, C reset coins, M cycle forced mission) + status label.
class DebugOverlayView : public BaseView {
public:
	bool init() override;

protected:
	void initListeners() override;

private:
	void refresh();
	ax::Label* m_label = nullptr;
	int m_forcedRoll = 0;
	bool m_fastBots = false;
	std::string m_forcedMission;
	std::string m_missionsInfo;
};

}  // namespace lm
