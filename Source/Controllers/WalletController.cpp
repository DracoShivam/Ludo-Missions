#include "Controllers/WalletController.h"

#include "axmol.h"
#include "Events/DebugEvents.h"
#include "Events/EventBus.h"
#include "Events/UiEvents.h"
#include "Events/WalletEvents.h"
#include "Utils/Log.h"

namespace lm {

static const char* COINS_KEY = "lm.coins";

WalletController* WalletController::sharedController() {
	static WalletController* s_instance = new WalletController();
	return s_instance;
}

void WalletController::init() {
	m_balance = ax::UserDefault::getInstance()->getIntegerForKey(COINS_KEY, 0);
	EventBus::subscribe<UiLobbyReady>(this, [this](const UiLobbyReady&) { publish(0, "sync"); });
	EventBus::subscribe<UiGameSceneReady>(this, [this](const UiGameSceneReady&) { publish(0, "sync"); });
	EventBus::subscribe<DebugResetCoins>(this, [this](const DebugResetCoins&) {
		m_balance = 0;
		ax::UserDefault::getInstance()->setIntegerForKey(COINS_KEY, 0);
		ax::UserDefault::getInstance()->flush();
		publish(0, "reset");
	});
}

void WalletController::add(int coins, const std::string& reason) {
	m_balance += coins;
	ax::UserDefault::getInstance()->setIntegerForKey(COINS_KEY, m_balance);
	ax::UserDefault::getInstance()->flush();
	LM_LOG("wallet +%d (%s) -> %d", coins, reason.c_str(), m_balance);
	publish(coins, reason);
}

void WalletController::publish(int delta, const std::string& reason) {
	WalletChanged e;
	e.balance = m_balance;
	e.delta = delta;
	e.reason = reason;
	EventBus::publish(e);
}

}  // namespace lm
