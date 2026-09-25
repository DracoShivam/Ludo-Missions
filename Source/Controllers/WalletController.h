#pragma once

#include <string>

namespace lm {

// Persistent coin balance (UserDefault "lm.coins"). Publishes WalletChanged.
class WalletController {
public:
	static WalletController* sharedController();
	void init();
	void add(int coins, const std::string& reason);
	int balance() const { return m_balance; }

private:
	WalletController() = default;
	void publish(int delta, const std::string& reason);
	int m_balance = 0;
};

}  // namespace lm
