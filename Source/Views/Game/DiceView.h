#pragma once

#include "axmol.h"

namespace lm {

// Dice face with roll animation. When `setTappable(true)` it pulses and publishes UiRollDiceTapped on tap.
class DiceView : public ax::Node {
public:
	bool init() override;
	void roll(int value, float duration);
	void setFace(int value);
	void setTappable(bool on);

private:
	ax::Sprite* m_sprite = nullptr;
	bool m_tappable = false;
};

}  // namespace lm
