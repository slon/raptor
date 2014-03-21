#include <raptor/core/scheduler.h>

#include <thread>

#include <raptor/core/impl.h>

namespace raptor {

struct scheduler_state_t {
	std::thread thread;
	scheduler_impl_t impl;
	bool running;

	void shutdown() {
		if(running) {
			impl.break_loop();
			thread.join();
			running = false;
		}
	}
};

scheduler_t::scheduler_t() : state_(std::make_shared<scheduler_state_t>()) {
	state_->running = true;
	state_->thread = std::thread([state_] () {
		state_->impl.run();
	});
}

fiber_t scheduler_t::start(std::function<void()> closure) {
	fiber_t fiber(std::move(closure));
	state_->impl.activate(fiber.get_impl());
	return fiber;
}

void scheduler_t::switch_to() {
	state_->impl.switch_to();
}

void scheduler_t::shutdown() {
	state_->impl.break_loop();
	state_->thread.join();
}

} // namespace raptor
