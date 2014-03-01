#pragma once

#include <memory>

#include <raptor/core/time.h>
#include <raptor/core/closure.h>

namespace raptor {

class fiber_t {
public:
	typedef std::shared_ptr<fiber_t> ptr;

	// Fiber can't be copied or moved
	fiber_t(const fiber_t&) = delete;
	fiber_t& operator = (const fiber_t&) = delete;
	fiber_t(fiber_t&&) = delete;
	fiber_t& operator = (fiber_t&&) = delete;

	// Destroyed fiber must be joined or detached
	~fiber_t();

	void join();
	void detach();

	void cancel();
};

} // namespace raptor
