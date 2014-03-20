#pragma once

#include <memory>

#include <raptor/core/time.h>
#include <raptor/core/closure.h>
#include <raptor/core/signal.h>

namespace raptor {

class fiber_impl_t;
struct fiber_state_t;

class fiber_t {
public:
	typedef std::shared_ptr<fiber_t> ptr;

	void join();

private:
	fiber_t(closure_t&& task);
	fiber_impl_t* get_impl();

	friend class scheduler_t;

	std::shared_ptr<fiber_state_t> state_;
};

} // namespace raptor
