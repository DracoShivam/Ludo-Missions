#pragma once

#include <string>

namespace lm {

struct WalletChanged {
	static constexpr const char* NAME = "lm.wallet.changed";
	int balance = 0;
	int delta = 0;
	std::string reason;
};

}  // namespace lm
