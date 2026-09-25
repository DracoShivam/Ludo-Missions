#include "Views/Common/BaseView.h"

#include "Events/EventBus.h"

namespace lm {

void BaseView::onEnter() {
	Node::onEnter();
	initListeners();
	afterEnter();
}

void BaseView::onExit() {
	EventBus::unsubscribeAll(this);
	beforeExit();
	Node::onExit();
}

void BaseScene::onEnter() {
	Scene::onEnter();
	initListeners();
	afterEnter();
}

void BaseScene::onEnterTransitionDidFinish() {
	Scene::onEnterTransitionDidFinish();
	afterTransition();
}

void BaseScene::onExit() {
	EventBus::unsubscribeAll(this);
	beforeExit();
	Scene::onExit();
}

}  // namespace lm
