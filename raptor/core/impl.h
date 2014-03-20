#pragma once

#include <atomic>
#include <mutex>
#include <vector>
#include <memory>

#include <ev.h>
#include <boost/intrusive/list.hpp>

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

namespace bi = boost::intrusive;

class fiber_impl_t : public bi::list_base_hook<> {
public:
	fiber_impl_t(closure_t* task, closure_t* terminate_cb = nullptr, size_t stack_size = 4 * 1024 * 1024);

	// [context:fiber]
	// switch to ev loop context, invoke deferred callbacks
	void yield(deferred_t* deferred = nullptr);

	// switch to fiber context
	void switch_to();

	bool is_terminated();

private:
	std::atomic<bool> terminated_;

	internal::context_t context_;
	closure_t* task_;
	closure_t* terminate_cb_;
	deferred_t* deferred_;
	std::unique_ptr<char[]> stack_;

	static void run_fiber(void* fiber);
};

struct monitor_t;

class scheduler_impl_t {
public:
	scheduler_impl_t();
	~scheduler_impl_t();

	// [context:ev] [thread:ev]
	void run(int flags = 0);
	void run_activated();

	// [context:any] [thread:any]
	void activate(fiber_impl_t* fiber);
	void break_loop();

	// [context:fiber] [thread:ev]
	enum wait_result_t {
		READY, TIMEDOUT
	};
 
	wait_result_t wait_io(int fd, int events, duration_t* timeout);
	wait_result_t wait_timeout(duration_t* timeout);
	wait_result_t wait_queue(spinlock_t* queue_lock, duration_t* timeout);

	void switch_to();

	void unlink_activate(fiber_impl_t* fiber);

private:
	struct ev_loop* ev_loop_;
	internal::context_t ev_context_;

	spinlock_t activated_lock_;
	bi::list<fiber_impl_t> activated_fibers_;
	ev_async activate_;

	ev_async break_loop_;

	friend class fiber_impl_t;
};

extern __thread fiber_impl_t* FIBER_IMPL;
extern __thread scheduler_impl_t* SCHEDULER_IMPL;

} // namespace raptor
