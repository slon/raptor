#pragma once

namespace raptor {

template<class x_t>
struct future_traits_t {
	template<class y_t, class fn_t>
	static void apply_and_set_value(
		promise_t<x_t>* promise,
		future_t<y_t>* future,
		fn_t* f
	) {
		promise->set_value((*f)(*future));
	}

	static void forward_value(
		promise_t<x_t>* promise,
		future_t<x_t>* future
	) {
		promise->set_value(future->get());
	}
};

template<>
struct future_traits_t<void> {
	template<class y_t, class fn_t>
	static void apply_and_set_value(
		promise_t<void>* promise,
		future_t<y_t>* future,
		fn_t* f
	) {
		(*f)(*future);
		promise->set_value();
	}

	static void forward_value(
		promise_t<void>* promise,
		future_t<void>* future
	) {
		promise->set_value();
	}
};

class shared_state_base_t {
public:
	shared_state_base_t() : queue_(&lock_), state_(EMPTY) {}

	std::exception_ptr get_exception() {
		wait(NULL);
		assert(state_ == EXCEPTION);
		return err_;
	}

	bool wait(duration_t* timeout) {
		std::lock_guard<spinlock_t> guard(lock_);

		while(state_ == EMPTY) {
			if(!queue_.wait(timeout)) return false;
		}

		return true;
	}

	void set_exception(std::exception_ptr err) {
		std::unique_lock<spinlock_t> guard(lock_);

		assert(state_ == EMPTY);
		state_ = EXCEPTION;
		err_ = err;

		notify_subscribers(guard);
	}

	bool is_ready() {
		std::lock_guard<spinlock_t> guard(lock_);
		return state_ != EMPTY;
	}

	bool has_value() {
		std::lock_guard<spinlock_t> guard(lock_);
		return state_ == VALUE;		
	}

	bool has_exception() {
		std::lock_guard<spinlock_t> guard(lock_);
		return state_ == EXCEPTION;		
	}

	void subscribe(closure_t closure) {
		std::unique_lock<spinlock_t> guard(lock_);

		if(state_ == EMPTY) {
			subscribers_.emplace_back(std::move(closure));
		} else {
			guard.unlock();
			closure.run();
		}
	}

protected:
	spinlock_t lock_;
	wait_queue_t queue_;

	enum state_t {
		EMPTY, VALUE, EXCEPTION
	};

	state_t state_;

	std::exception_ptr err_;
	std::vector<closure_t> subscribers_;

	void notify_subscribers(std::unique_lock<spinlock_t>& guard) {
		queue_.notify_all();

		std::vector<closure_t> subscribers;
		subscribers.swap(subscribers_);

		guard.unlock();

		for(const auto& closure : subscribers) {
			closure.run();
		}
	}
};

template<class x_t>
class shared_state_t : public shared_state_base_t {
public:
	const x_t& get() {
		wait(nullptr);

		if(state_ == EXCEPTION) {
			std::rethrow_exception(err_);
		} else {
			return *value_;
		}
	}

	void set_value(const x_t& value) {
		std::unique_lock<spinlock_t> guard(lock_);

		assert(state_ == EMPTY);
		state_ = VALUE;
		value_.reset(new x_t(value));

		notify_subscribers(guard);
	}

	friend class future_t<x_t>;
	friend class promise_t<x_t>;

private:
	std::unique_ptr<x_t> value_;
};

template<>
class shared_state_t<void> : public shared_state_base_t {
public:
	void get() {
		wait(nullptr);

		if(state_ == EXCEPTION) {
			std::rethrow_exception(err_);
		}
	}

	void set_value() {
		std::unique_lock<spinlock_t> guard(lock_);

		assert(state_ == EMPTY);
		state_ = VALUE;

		notify_subscribers(guard);
	}

	friend class future_t<void>;
	friend class promise_t<void>;
};

template<class x_t>
typename future_t<x_t>::x_const_ref_t future_t<x_t>::get() const {
	assert(state_);
	return state_->get();
}

template<class x_t>
std::exception_ptr future_t<x_t>::get_exception() const {
	assert(state_);
	return state_->get_exception();
}

template<class x_t>
bool future_t<x_t>::is_valid() const {
	return state_ != nullptr;
}

template<class x_t>
bool future_t<x_t>::is_ready() const {
	assert(state_);
	return state_->is_ready();
}

template<class x_t>
bool future_t<x_t>::has_value() const {
	assert(state_);
	return state_->has_value();
}

template<class x_t>
bool future_t<x_t>::has_exception() const {
	assert(state_);
	return state_->has_exception();
}

template<class x_t>
bool future_t<x_t>::wait(duration_t* timeout) const {
	assert(state_);
	return state_->wait(timeout);
}

template<class x_t>
template<class fn_t>
auto future_t<x_t>::then(fn_t&& fn) -> future_t<decltype(fn(std::declval<future_t<x_t>>()))> const {
	return then(nullptr, fn);
}

template<class x_t>
template<class fn_t>
auto future_t<x_t>::then(executor_t* executor, fn_t&& fn) -> future_t<decltype(fn(std::declval<future_t<x_t>>()))> const {
	assert(state_);
	typedef decltype(fn(future_t<x_t>())) y_t;

	promise_t<y_t> chained_promise;
	future_t<y_t> chained_future = chained_promise.get_future();

	auto state = state_;
	closure_t subscriber(executor, [fn, chained_promise, state] () mutable {
		future_t<x_t> future(state);

		try {
			future_traits_t<y_t>::apply_and_set_value(&chained_promise, &future, &fn);
		} catch(...) {
			chained_promise.set_exception(std::current_exception());
		}
	});

	state_->subscribe(subscriber);
	return chained_future;
}

template<class x_t>
template<class fn_t>
auto future_t<x_t>::bind(fn_t&& fn) -> decltype(fn(std::declval<future_t<x_t>>())) const {
	return bind(nullptr, fn);
}

template<class x_t>
template<class fn_t>
auto future_t<x_t>::bind(executor_t* executor, fn_t&& fn) -> decltype(fn(std::declval<future_t<x_t>>())) const {
	assert(state_);
	typedef decltype(fn(future_t<x_t>())) result_future_t;
	typedef typename result_future_t::value_t y_t;

	promise_t<y_t> chained_promise;
	std::function<void(future_t<y_t>)> forward_handler = [chained_promise] (future_t<y_t> future) mutable {
		if(future.has_exception()) {
			chained_promise.set_exception(future.get_exception());
		} else {
			future_traits_t<y_t>::forward_value(&chained_promise, &future);
		}
	};

	auto state = state_;
	closure_t subscriber(executor, [fn, chained_promise, forward_handler, state] () mutable {
		future_t<x_t> this_future(state);

		future_t<y_t> outer_future;
		try {
			outer_future = fn(this_future);
			outer_future.then(forward_handler);
		} catch(...) {
			chained_promise.set_exception(std::current_exception());
		}
	});

	state_->subscribe(subscriber);
	return chained_promise.get_future();
}

template<class x_t>
template<class fn_t>
void future_t<x_t>::subscribe(fn_t&& fn) const {
	subscribe(nullptr, fn);
}

template<class x_t>
template<class fn_t>
void future_t<x_t>::subscribe(executor_t* executor, fn_t&& fn) const {
	auto state = state_;
	closure_t subscriber(executor, [fn, state] () {
		future_t<x_t> this_future(state);

		try {
			fn(this_future);
		} catch(...) {
			abort();
		}
	});

	state_->subscribe(subscriber);
}

template<class x_t>
promise_t<x_t>::promise_t() : state_(std::make_shared<shared_state_t<x_t>>()) {}

template<class x_t>
future_t<x_t> promise_t<x_t>::get_future() const {
	return future_t<x_t>(state_);
}

template<class x_t>
void promise_t<x_t>::set_value() {
	state_->set_value();
}

template<class x_t>
template<class y_t>
void promise_t<x_t>::set_value(const y_t& value) {
	state_->set_value(value);
}

template<class x_t>
void promise_t<x_t>::set_exception(std::exception_ptr err) {
	state_->set_exception(err);
}

template<class x_t>
future_t<x_t> make_ready_future(const x_t& x) {
	promise_t<x_t> promise;
	promise.set_value(x);
	return promise.get_future();
}

inline future_t<void> make_ready_future() {
	promise_t<void> promise;
	promise.set_value();
	return promise.get_future();
}

template<class x_t>
future_t<x_t> make_exception_future(std::exception_ptr err) {
	promise_t<x_t> promise;
	promise.set_exception(err);
	return promise.get_future();
}

} // namespace raptor
