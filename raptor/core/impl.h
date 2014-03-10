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

struct wakeup_list_tag_t;
struct ownership_list_tag_t;

namespace bi = boost::intrusive;

typedef bi::list_base_hook<
	bi::tag<wakeup_list_tag_t>,
	bi::link_mode<bi::auto_unlink>
> wakeup_list_hook_t;

typedef bi::list_base_hook<
	bi::tag<ownership_list_tag_t>,
	bi::link_mode<bi::auto_unlink>
> ownership_list_hook_t;

class fiber_impl_t : public wakeup_list_hook_t, public ownership_list_hook_t {
public:
	fiber_impl_t(closure_t task, size_t stack_size = 4 * 1024 * 1024);

	enum fiber_state_t {
		RUNNING, WAITING, TIMEDOUT, CANCELED, TERMINATED
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
	fiber_state_t state();

private:
	spinlock_t lock_;

	// protected by lock
	fiber_state_t state_;
	scheduler_impl_t* scheduler_;

	// accessed only from fiber thread
	internal::context_t context_;
	closure_t task_;
	deferred_t* deferred_;
	std::unique_ptr<char[]> stack_;

	static void run_fiber(void* fiber);
};

typedef bi::list<
	fiber_impl_t,
	bi::constant_time_size<false>,
	bi::base_hook<wakeup_list_hook_t>
> wakeup_list_t;

typedef bi::list<
	fiber_impl_t,
	bi::constant_time_size<false>,
	bi::base_hook<wakeup_list_hook_t>
> ownership_list_t;

class scheduler_impl_t {
public:
	scheduler_impl_t();
	~scheduler_impl_t();

	void run();
	void run_once();
	void cancel();

	// [context:any] [thread:any]
	void wakeup(fiber_impl_t* fiber);

	// [context:fiber] [thread:ev]
	void wait_io(int fd, int events);
	void wait_timer(duration_t t);

private:
	struct ev_loop* ev_loop_;
	internal::context_t context_;

	ev_async wakeup_;
	ev_prepare prepare_;

	ownership_list_t owned_fibers_;

	std::mutex wakeup_list_lock_;
	wakeup_list_t woken_up_fibers_;

	friend class fiber_impl_t;
};

extern thread_local fiber_impl_t* FIBER_IMPL;
extern thread_local scheduler_impl_t* SCHEDULER_IMPL;

} // namespace raptor
