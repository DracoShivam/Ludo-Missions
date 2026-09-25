#pragma once

#include "Views/Common/BaseView.h"

namespace lm {

class LobbyScene : public BaseScene {
public:
	bool init() override;

protected:
	void afterTransition() override;
};

}  // namespace lm
