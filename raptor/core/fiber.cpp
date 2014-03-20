#include <raptor/core/fiber.h>

#include <raptor/core/impl.h>
#include <raptor/core/signal.h>

namespace raptor {

struct fiber_state_t {
	fiber_state_t(closure_t&& _task) :
		task(std::move(_task)),
		terminate_cb([this] () {
			terminated.signal();
			this_ptr.reset();
		}),
		impl(&task, &terminate_cb) {}

	signal_t terminated;

	closure_t task;
	closure_t terminate_cb;

	fiber_impl_t impl;

	std::shared_ptr<fiber_state_t> this_ptr;
};

fiber_t::fiber_t(closure_t&& task) :
		state_(std::make_shared<fiber_state_t>(std::move(task))) {
	state_->this_ptr = state_;
}

void fiber_t::join() {
	assert(state_);

	state_->terminated.wait();
}

fiber_impl_t* fiber_t::get_impl() {
	assert(state_);

	return &(state_->impl);
}

} // namespace raptor
