#pragma once

#include <cstdio>

// Pure logging (no axmol) so it works in the app and in headless tests.
#define LM_LOG(fmt, ...) std::fprintf(stderr, "[LM] " fmt "\n" __VA_OPT__(, ) __VA_ARGS__)
#define LM_LOG_ERROR(fmt, ...) std::fprintf(stderr, "[LM][ERROR] " fmt "\n" __VA_OPT__(, ) __VA_ARGS__)
#define LM_LOG_WARN(fmt, ...) std::fprintf(stderr, "[LM][WARN] " fmt "\n" __VA_OPT__(, ) __VA_ARGS__)
