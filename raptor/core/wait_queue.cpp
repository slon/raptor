#include <raptor/core/wait_queue.h>

#include <condition_variable>

#include <raptor/core/impl.h>

namespace raptor {

void queue_waiter_t::wakeup() {}

struct fiber_waiter_t : public queue_waiter_t {
	fiber_waiter_t(fiber_impl_t* fiber, scheduler_impl_t* scheduler)
		: fiber(fiber), scheduler(scheduler) {}

	fiber_impl_t* fiber;
	scheduler_impl_t* scheduler;

	virtual void wakeup() {
		scheduler->activate(fiber);
	}
};

struct native_waiter_t : public queue_waiter_t {
	std::mutex mutex;
	std::condition_variable ready;

	spinlock_t* queue_lock;

	bool wait(duration_t* timeout) {
		std::unique_lock<std::mutex> guard(mutex);
		queue_lock->unlock();

		std::cv_status res = std::cv_status::no_timeout;

		if(timeout) {
			auto start = std::chrono::system_clock::now();
			res = ready.wait_for(guard, *timeout);	
			*timeout -= (std::chrono::system_clock::now() - start);
		} else {
			ready.wait(guard);
		}

		guard.unlock();
		queue_lock->lock();

		return res == std::cv_status::no_timeout;
	}

	virtual void wakeup() {
		std::lock_guard<std::mutex> guard(mutex);
		ready.notify_one();
	}
};

bool wait_queue_t::wait(duration_t* timeout) {
	if(FIBER_IMPL) {
		fiber_waiter_t waiter(FIBER_IMPL, SCHEDULER_IMPL);
		waiters_.push_back(waiter);

		auto wait_res = SCHEDULER_IMPL->wait_queue(lock_, timeout);

		waiters_.erase(waiters_.iterator_to(waiter));

		if(waiter.wakeup_next)
			notify_one();

		return wait_res == scheduler_impl_t::READY;
	} else {
		native_waiter_t waiter;
		waiter.queue_lock = lock_;

		waiters_.push_back(waiter);

		bool wait_successfull = waiter.wait(timeout);

		waiters_.erase(waiters_.iterator_to(waiter));

		if(waiter.wakeup_next)
			notify_one();

		return wait_successfull;
	}
}

void wait_queue_t::notify_one() {
	if(!waiters_.empty()) {
		waiters_.begin()->wakeup();
	}
}

void wait_queue_t::notify_all() {
	if(!waiters_.empty()) {
		auto last = --waiters_.end();
		for(auto waiter_it = waiters_.begin(); waiter_it != last; ++waiter_it) {
			waiter_it->wakeup_next = true;
		}
		waiters_.begin()->wakeup();
	}	
}

} // namespace raptor
