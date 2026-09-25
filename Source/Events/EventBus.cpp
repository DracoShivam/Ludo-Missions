#include "Events/EventBus.h"

namespace lm {

std::unordered_map<void*, std::vector<ax::EventListener*>> EventBus::s_listeners;

void EventBus::unsubscribeAll(void* owner) {
	auto it = s_listeners.find(owner);
	if (it == s_listeners.end()) {
		return;
	}
	auto* dispatcher = ax::Director::getInstance()->getEventDispatcher();
	for (auto* l : it->second) {
		dispatcher->removeEventListener(l);
	}
	s_listeners.erase(it);
}

}  // namespace lm
