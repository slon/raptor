#include <raptor/core/monitor.h>

#include <condition_variable>

#include <raptor/core/impl.h>

namespace raptor {

void monitor_waiter_t::wakeup() {}

struct fiber_monitor_waiter_t : public monitor_waiter_t {
	fiber_monitor_waiter_t(fiber_impl_t* fiber, scheduler_impl_t* scheduler)
		: fiber(fiber), scheduler(scheduler) {}

	fiber_impl_t* fiber;
	scheduler_impl_t* scheduler;

	virtual void wakeup() {
		scheduler->activate(fiber);
	}
};

struct native_monitor_waiter_t : public monitor_waiter_t {
	std::mutex lock;
	std::condition_variable ready;

	void wait(duration_t* timeout) {}

	virtual void wakeup() {}
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

void monitor_t::notify_one() {
	if(!waiters_.empty()) {
		waiters_.begin()->wakeup();
	}
}

void monitor_t::notify_all() {
	for(monitor_waiter_t& waiter : waiters_) {
		waiter.wakeup();
	}
}

} // namespace raptor
