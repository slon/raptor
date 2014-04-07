#pragma once

#include <chrono>

namespace raptor {

typedef std::chrono::duration<double, std::chrono::seconds::period> duration_t;
typedef std::chrono::time_point<std::chrono::system_clock, duration_t> time_point_t;

} // namespace raptor
