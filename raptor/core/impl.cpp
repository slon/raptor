#include <raptor/core/impl.h>

#include <cassert>

namespace raptor {

void fiber_impl_t::run_fiber(void* arg) {
	fiber_impl_t* fiber = (fiber_impl_t*)arg;
	fiber->task_();

	std::unique_lock<spinlock_t> guard(fiber->lock_);
	fiber->run_state_ = TERMINATED;
	guard.unlock();

	fiber->yield(nullptr);
}

fiber_impl_t::fiber_impl_t(closure_t task, size_t stack_size) :
		run_state_(SUSPENDED),
		woken_up_(false),
		scheduler_(nullptr),
		task_(task),
		deferred_(nullptr),
		stack_(new char[stack_size]) {
	context_.create(stack_.get(), stack_size, run_fiber, this);
}

void fiber_impl_t::yield(deferred_t* deferred) {
	deferred_ = deferred;
	context_.switch_to(&SCHEDULER_IMPL->ev_context_);
}

struct deferred_send_t : public deferred_t {
	deferred_send_t(fiber_impl_t* fiber)
		: fiber(fiber) {}

	fiber_impl_t* fiber;

	virtual void after_yield() {
		fiber->wakeup();
	}
};

fiber_impl_t::run_state_t fiber_impl_t::state() {
	return run_state_;
}

void fiber_impl_t::jump_to(scheduler_impl_t* scheduler) {
	deferred_send_t deferred_send(this);

	std::lock_guard<spinlock_t> guard(lock_);
	scheduler_ = scheduler;

	yield(&deferred_send);
}

void fiber_impl_t::switch_to() {
	if(deferred_) {
		deferred_->before_switch_to();
		deferred_ = nullptr;
	}

	FIBER_IMPL = this;
	scheduler_ = SCHEDULER_IMPL;
	SCHEDULER_IMPL->ev_context_.switch_to(&context_);
	FIBER_IMPL = nullptr;

	if(deferred_) {
		deferred_->after_yield();
	}
}

void fiber_impl_t::wakeup() {
	std::unique_lock<spinlock_t> guard(lock_);
	assert(run_state_ != TERMINATED);
	if(!woken_up_) {
		woken_up_ = true;
	} else {
		return;
	}
	guard.unlock();

	scheduler_->activate(this);
}

static void activate_cb(struct ev_loop* loop, ev_async*, int) {
	scheduler_impl_t* scheduler = (scheduler_impl_t*)ev_userdata(loop);
	scheduler->run_activated();
}

static void break_loop_cb(struct ev_loop* loop, ev_async*, int) {
	ev_break(loop, EVBREAK_ONE);
}

scheduler_impl_t::scheduler_impl_t() {
	ev_loop_ = ev_loop_new(0);
	ev_set_userdata(ev_loop_, this);

	ev_async_init(&activate_, activate_cb);
	ev_async_start(ev_loop_, &activate_);

	ev_async_init(&break_loop_, break_loop_cb);
	ev_async_start(ev_loop_, &break_loop_);
}

scheduler_impl_t::~scheduler_impl_t() {
	ev_async_stop(ev_loop_, &activate_);
	ev_async_stop(ev_loop_, &break_loop_);
	ev_loop_destroy(ev_loop_);
}

void scheduler_impl_t::run_activated() {
	std::unique_lock<std::mutex> guard(activated_mutex_);
	while(!activated_fibers_.empty()) {
		fiber_impl_t* fiber = activated_fibers_.back();
		activated_fibers_.pop_back();
		guard.unlock();

		fiber->switch_to();

		guard.lock();
	}
}

void scheduler_impl_t::run(int flags) {
	SCHEDULER_IMPL = this;
	ev_run(ev_loop_, flags);
	SCHEDULER_IMPL = nullptr;
}

void scheduler_impl_t::break_loop() {
	ev_async_send(ev_loop_, &break_loop_);	
}

struct watcher_data_t {
	watcher_data_t(fiber_impl_t* fiber) : fiber(fiber), events(0) {}

	fiber_impl_t* fiber;
	int events;
};

static void switch_to_cb(struct ev_loop* loop, ev_watcher* io, int events) {
	watcher_data_t* data = (watcher_data_t*)io->data;
	data->events = events;
	data->fiber->switch_to();
}

scheduler_impl_t::wait_result_t scheduler_impl_t::wait_io(int fd, int events, duration_t* timeout) {
	ev_io io_ready;
	ev_timer timer_timeout;

	watcher_data_t watcher_data(FIBER_IMPL);

	ev_init((ev_watcher*)&io_ready, switch_to_cb);
	ev_io_set(&io_ready, fd, events);
	io_ready.data = &watcher_data;
	ev_io_start(ev_loop_, &io_ready);

	if(timeout) {
		ev_init((ev_watcher*)&timer_timeout, switch_to_cb);
		ev_timer_set(&timer_timeout, timeout->count(), 0.0);
		timer_timeout.data = &watcher_data;
		ev_timer_start(ev_loop_, &timer_timeout);
	}

	ev_tstamp start_wait = ev_now(ev_loop_);
	FIBER_IMPL->yield();
	if(timeout)
		*timeout -= duration_t(ev_now(ev_loop_) - start_wait);

	ev_io_stop(ev_loop_, &io_ready);
	if(timeout) {
		ev_timer_stop(ev_loop_, &timer_timeout);
	}

	if(watcher_data.events & EV_TIMER) {
		return TIMEDOUT;
	} else {
		return READY;
	}
}

void scheduler_impl_t::activate(fiber_impl_t* fiber) {
	std::unique_lock<std::mutex> guard(activated_mutex_);
	activated_fibers_.push_back(fiber);
	guard.unlock();

	ev_async_send(ev_loop_, &activate_);
}

scheduler_impl_t::wait_result_t scheduler_impl_t::wait_timeout(duration_t* timeout) {
	assert(timeout);

	ev_timer timer_ready;

	watcher_data_t watcher_data(FIBER_IMPL);

	ev_init((ev_watcher*)&timer_ready, switch_to_cb);
	ev_timer_set(&timer_ready, timeout->count(), 0.0);
	ev_timer_start(ev_loop_, &timer_ready);
	timer_ready.data = &watcher_data;

	ev_tstamp start_wait = ev_now(ev_loop_);
	FIBER_IMPL->yield();
	*timeout -= duration_t(ev_now(ev_loop_) - start_wait);

	ev_timer_stop(ev_loop_, &timer_ready);

	return READY;
}

thread_local fiber_impl_t* FIBER_IMPL = nullptr;
thread_local scheduler_impl_t* SCHEDULER_IMPL = nullptr;

} // namespace raptor
