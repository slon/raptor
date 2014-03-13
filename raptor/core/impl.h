#pragma once

#include <atomic>
#include <mutex>
#include <vector>
#include <memory>

#include <ev.h>

#include <raptor/core/spinlock.h>
#include <raptor/core/time.h>
#include <raptor/core/context.h>
#include <raptor/core/closure.h>

namespace raptor {

class scheduler_impl_t;

class deferred_t {
public:
	virtual void after_yield() {}
	virtual void before_switch_to() {}
};

class fiber_impl_t {
public:
	fiber_impl_t(closure_t task, size_t stack_size = 4 * 1024 * 1024);

	enum run_state_t {
		RUNNING, SUSPENDED, TERMINATED
	};

	// [context:fiber]
	void yield(deferred_t* deferred = nullptr);
	void check_state();
	void jump_to(scheduler_impl_t* scheduler);

	// [context:ev]
	void switch_to();

	// [context:any] [thread:any]
	void wakeup();
	void wakeup_with_cancel();
	void wakeup_with_timeout();
	run_state_t state();

private:
	spinlock_t lock_;

	// protected by lock
	run_state_t run_state_;
	bool timedout_;
	bool cancaled_;

	scheduler_impl_t* scheduler_;

	// accessed only from fiber thread
	internal::context_t context_;
	closure_t task_;
	deferred_t* deferred_;
	std::unique_ptr<char[]> stack_;

	static void run_fiber(void* fiber);
};

class scheduler_impl_t {
public:
	scheduler_impl_t();
	~scheduler_impl_t();

	void run(int flags = 0);
	void run_activated();
	void cancel();

	// [context:any] [thread:any]
	void activate(fiber_impl_t* fiber);

	// [context:fiber] [thread:ev]
	void wait_io(int fd, int events, duration_t* timeout);
	void wait_timeout(duration_t* timeout);

private:
	struct ev_loop* ev_loop_;
	internal::context_t ev_context_;

	std::vector<fiber_impl_t*> owned_fibers_;

	std::mutex activated_mutex_;
	std::vector<fiber_impl_t*> activated_fibers_;
	ev_async activate_;

	ev_async cancel_;

	friend class fiber_impl_t;
};

extern thread_local fiber_impl_t* FIBER_IMPL;
extern thread_local scheduler_impl_t* SCHEDULER_IMPL;

} // namespace raptor
