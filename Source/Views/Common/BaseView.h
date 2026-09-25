#pragma once

#include "axmol.h"

namespace lm {

// Node base: subscribes in initListeners() on enter, auto-unsubscribes everything on exit.
// Children's onEnter runs inside Node::onEnter(), so by afterEnter() all child views are subscribed:
// publish "ready" events only from afterEnter().
class BaseView : public ax::Node {
public:
	void onEnter() final;
	void onExit() final;

protected:
	virtual void initListeners() {}
	virtual void afterEnter() {}
	virtual void beforeExit() {}
};

class BaseScene : public ax::Scene {
public:
	void onEnter() final;
	void onExit() final;

protected:
	virtual void initListeners() {}
	virtual void afterEnter() {}
	virtual void beforeExit() {}
};

}  // namespace lm
