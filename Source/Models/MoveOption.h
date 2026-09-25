#pragma once

namespace lm {

struct MoveOption {
	int player = -1;
	int token = -1;
	int value = 0;  // dice value used
	int from = 0;
	int to = 0;
	bool captures = false;
};

}  // namespace lm
