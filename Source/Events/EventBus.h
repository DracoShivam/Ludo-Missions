#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "axmol.h"

namespace lm {

// Typed wrapper over axmol's EventDispatcher. Every event is a struct with `static constexpr const char* NAME`.
// Dispatch is SYNCHRONOUS; the payload pointer is only valid inside the callback (copy what you need).
//   EventBus::subscribe<UiPlayTapped>(this, [this](const UiPlayTapped&) { ... });   // <T> is required
//   EventBus::publish(UiPlayTapped{});
//   EventBus::unsubscribeAll(this);   // BaseView/BaseScene do this in onExit; controllers in reset()
class EventBus {
public:
	template <class T>
	static void publish(const T& e) {
		ax::Director::getInstance()->getEventDispatcher()->dispatchCustomEvent(T::NAME, const_cast<T*>(&e));
	}

	template <class T>
	static void subscribe(void* owner, std::function<void(const T&)> fn) {
		auto* l = ax::EventListenerCustom::create(T::NAME, [fn](ax::EventCustom* ev) { fn(*static_cast<const T*>(ev->getUserData())); });
		ax::Director::getInstance()->getEventDispatcher()->addEventListenerWithFixedPriority(l, 1);  // never 0 (asserts)
		s_listeners[owner].push_back(l);
	}

	static void unsubscribeAll(void* owner);
	static size_t ownerCount() { return s_listeners.size(); }

private:
	static std::unordered_map<void*, std::vector<ax::EventListener*>> s_listeners;
};

}  // namespace lm
