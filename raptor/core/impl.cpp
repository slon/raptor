#include <raptor/core/impl.h>

namespace raptor {

fiber_impl_t::fiber_impl_t(closure_t task, size_t stack_size) {}

void fiber_impl_t::yield(deferred_t* deferred) {
	deferred_ = deferred;
	context_.switch_to(&SCHEDULER_IMPL->context_);
}

struct deferred_send_t : public deferred_t {
	deferred_send_t(scheduler_impl_t* scheduler, fiber_impl_t* fiber)
		: scheduler(scheduler), fiber(fiber) {}

	scheduler_impl_t* scheduler;
	fiber_impl_t* fiber;

	virtual void after_yield() {
		scheduler->wakeup(fiber);
	}
};

fiber_impl_t::fiber_state_t fiber_impl_t::state() {
	return RUNNING;
}

void fiber_impl_t::jump_to(scheduler_impl_t* scheduler) {
	deferred_send_t deferred_send{scheduler, this};

	scheduler_ = scheduler;

	yield(&deferred_send);
}

void fiber_impl_t::check_state() {}

void fiber_impl_t::switch_to() {
	if(deferred_) {
		deferred_->before_switch_to();
		deferred_ = nullptr;
	}

	FIBER_IMPL = this;
	SCHEDULER_IMPL->context_.switch_to(&context_);
	FIBER_IMPL = nullptr;

	if(deferred_) {
		deferred_->after_yield();
	}
}

void fiber_impl_t::wakeup_with_cancel() {
	state_ = CANCELED;
	wakeup();
}

void fiber_impl_t::wakeup_with_timeout() {
	state_ = TIMEDOUT;
	wakeup();
}

void fiber_impl_t::wakeup() {
	scheduler_->wakeup(this);
}

static void async_cb(struct ev_loop* loop, ev_async* async, int) {

}

scheduler_impl_t::scheduler_impl_t() {
	ev_loop_ = ev_loop_new(0);

	ev_async_init(&wakeup_, async_cb);
	ev_async_start(ev_loop_, &wakeup_);
}

scheduler_impl_t::~scheduler_impl_t() {
	ev_async_stop(ev_loop_, &wakeup_);
	ev_loop_destroy(ev_loop_);
}

void scheduler_impl_t::run() {
	throw std::runtime_error("not implemented");
}

void scheduler_impl_t::run_once() {
	throw std::runtime_error("not implemented");
}

void scheduler_impl_t::cancel() {
	throw std::runtime_error("not implemented");
}

static void switch_to_cb(struct ev_loop* loop, ev_watcher* io, int) {
	fiber_impl_t* fiber = (fiber_impl_t*)io->data;
	fiber->switch_to();
}

void scheduler_impl_t::wait_io(int fd, int events) {
	ev_io io_ready;

	FIBER_IMPL->check_state();

	ev_init((ev_watcher*)&io_ready, switch_to_cb);
	ev_io_set(&io_ready, fd, events);
	ev_io_start(ev_loop_, &io_ready);
	io_ready.data = FIBER_IMPL;

	FIBER_IMPL->yield();

	ev_io_stop(ev_loop_, &io_ready);

	FIBER_IMPL->check_state();
}

double duration_to_ev_time(duration_t t) { return 0.0; }

void scheduler_impl_t::wakeup(fiber_impl_t* fiber) {}

void scheduler_impl_t::wait_timer(duration_t t) {
	ev_timer timer_ready;

	FIBER_IMPL->check_state();

	ev_init((ev_watcher*)&timer_ready, switch_to_cb);
	ev_timer_set(&timer_ready, duration_to_ev_time(t), 0.0);
	ev_timer_start(ev_loop_, &timer_ready);
	timer_ready.data = FIBER_IMPL;

	FIBER_IMPL->yield();

	ev_timer_stop(ev_loop_, &timer_ready);

	FIBER_IMPL->check_state();
}

thread_local fiber_impl_t* FIBER_IMPL = nullptr;
thread_local scheduler_impl_t* SCHEDULER_IMPL = nullptr;

} // namespace raptor
