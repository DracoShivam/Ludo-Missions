#pragma once

#include "axmol.h"

namespace lm {

// Dice face with roll animation. When `setTappable(true)` it pulses and publishes UiRollDiceTapped on tap.
class DiceView : public ax::Node {
public:
	bool init() override;
	void roll(int value, float duration);
	void setFace(int value);
	// `raw` is what the die showed, `spend` what the roll is worth. They differ only when a power
	// modified it, and the badge is the player's proof that it did.
	void setFaceWithModifier(int raw, int spend);
	// Tumble to the real die face, then reveal what a power turned it into.
	void rollModified(int raw, int spend, float duration);
	void setTappable(bool on);

private:
	ax::Sprite* m_sprite = nullptr;
	ax::Label* m_badge = nullptr;
	int m_pendingRaw = 6;
	int m_pendingSpend = 6;
	bool m_tappable = false;
};

}  // namespace lm
