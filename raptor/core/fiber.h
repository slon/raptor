#pragma once

#include <memory>

#include <raptor/core/time.h>
#include <raptor/core/signal.h>

namespace raptor {

class fiber_impl_t;
struct fiber_state_t;

class fiber_t {
public:
	fiber_t() {}

	bool is_valid() { return state_ != nullptr; }

	void join();

private:
	fiber_t(std::function<void()> task);
	fiber_impl_t* get_impl();

	friend class scheduler_t;

	std::shared_ptr<fiber_state_t> state_;
};

} // namespace raptor
