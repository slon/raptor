#include <raptor/core/monitor.h>

#include <raptor/core/impl.h>

namespace raptor {

struct fiber_monitor_waiter_t : public monitor_waiter_t {
	fiber_monitor_waiter_t(fiber_impl_t* fiber, scheduler_impl_t* scheduler)
		: fiber(fiber), scheduler(scheduler), activated(false) {}

	fiber_impl_t* fiber;
	scheduler_impl_t* scheduler;
	bool activated;

	virtual void wakeup() {
		if(!activated) {
			activated = true;
			scheduler->activate(fiber);
		}
	}
};

bool monitor_t::wait(duration_t* timeout) {
	if(FIBER_IMPL) {
		fiber_monitor_waiter_t waiter(FIBER_IMPL, SCHEDULER_IMPL);
		waiters_.push_back(waiter);

		auto wait_res = SCHEDULER_IMPL->wait_monitor(this, timeout);

		waiters_.erase(waiters_.iterator_to(waiter));
		return wait_res == scheduler_impl_t::READY;
	} else {
		assert(!"not implemented yet");
	}
}

} // namespace raptor
