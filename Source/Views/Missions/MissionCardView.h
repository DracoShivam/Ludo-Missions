#pragma once

#include "axmol.h"
#include "Models/MissionInstance.h"

namespace lm {

// One active mission: title, description, progress bar, x/y, turns left, reward.
class MissionCardView : public ax::Node {
public:
	static MissionCardView* create(const MissionInstance& m);
	bool initWith(const MissionInstance& m);
	void update(const MissionInstance& m);
	void playCompleted();
	void playFailed();
	ax::Vec2 rewardWorldPosition() const;
	int uid() const { return m_uid; }

private:
	void drawBar(float ratio, ax::Color4B color);
	int m_uid = 0;
	ax::Node* m_bg = nullptr;
	ax::Label* m_desc = nullptr;
	ax::Label* m_count = nullptr;
	ax::Label* m_turns = nullptr;
	ax::Label* m_power = nullptr;
	ax::Node* m_reward = nullptr;
	ax::DrawNode* m_bar = nullptr;
	float m_ratio = 0;
};

}  // namespace lm
