#include <raptor/core/scheduler.h>

#include <thread>

#include <raptor/core/impl.h>

namespace raptor {

struct scheduler_state_t {
	std::thread thread;
	scheduler_impl_t impl;
};

scheduler_t::scheduler_t() : state_(new scheduler_state_t()) {
	state_->thread = std::thread([this] () {
		state_->impl.run();
	});
}

scheduler_t::~scheduler_t() { shutdown(); }

fiber_t scheduler_t::start(std::function<void()> closure) {
	fiber_t fiber(std::move(closure));
	state_->impl.activate(fiber.get_impl());
	return fiber;
}

void scheduler_t::switch_to() {
	state_->impl.switch_to();
}

void scheduler_t::shutdown() {
	if(state_->thread.joinable()) {
		state_->impl.break_loop();
		state_->thread.join();
	}
}

} // namespace raptor
