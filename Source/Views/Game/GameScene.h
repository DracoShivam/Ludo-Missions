#pragma once

#include <string>
#include <vector>

#include "Views/Common/BaseView.h"

namespace lm {

class GameScene : public BaseScene {
public:
	bool init() override;

protected:
	void initListeners() override;
	void afterEnter() override;
	void beforeExit() override;

private:
	void showResult();
	std::vector<int> m_ranking;
	std::vector<std::string> m_names;
	int m_self = 0;
	float m_resultDelay = 1.5f;
	int m_missionsCompleted = 0;  // filled from MissionMatchSummary (P6)
	int m_coinsEarned = 0;
};

}  // namespace lm
