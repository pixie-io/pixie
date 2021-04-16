#pragma once

#include <grpcpp/grpcpp.h>

namespace px {
/**
 * @brief Sleeps for specified period of time, slowing down based on the build configuration (ie
 * ASAN, TSAN, etc)
 * @param ms_to_wait
 */
void TestSleep(int64_t ms_to_wait);
}  // namespace px
