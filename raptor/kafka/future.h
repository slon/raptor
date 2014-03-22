#pragma once

#include <exception>
#include <functional>
#include <stdexcept>
#include <memory>
#include <atomic>
#include <vector>

#include <pd/base/time.H>
#include <pd/base/exception.H>
#include <pd/base/ref.H>

#include <pd/bq/bq_cond.H>

#include <phantom/pd.H>

namespace phantom {

template<class x_t> class future_traits_t;

template<class x_t> class future_t;
template<class x_t> class promise_t;

typedef std::function<void()> closure_t;

class shared_state_base_t : public ref_count_atomic_t {
public:
	shared_state_base_t() : state_(EMPTY) {}

	shared_state_base_t(shared_state_base_t&& ) = delete;
	shared_state_base_t(const shared_state_base_t& ) = delete;

	shared_state_base_t& operator = (shared_state_base_t&& ) = delete;
	shared_state_base_t& operator = (const shared_state_base_t& ) = delete;

	std::exception_ptr get_exception() {
		wait(NULL);
		assert(state_ == EXCEPTION);
		return err_;
	}

	void wait(interval_t* interval) {
		bq_cond_t::handler_t handler(ready_cond_);

		while(state_ == EMPTY) {
			if(!bq_success(handler.wait(interval)) && errno != ETIMEDOUT) {
				throw exception_sys_t(log::error, errno, "shared_state_t<void>::wait(): %m");
			}
		}
	}

	void set_exception(std::exception_ptr err) {
		bq_cond_t::handler_t handler(ready_cond_);

		assert(state_ == EMPTY);
		state_ = EXCEPTION;
		err_ = err;

		handler.send(true);
		ready_cond_.unlock();
		notify_subscribers();
	}

	bool is_ready() { return state_ != EMPTY; }

	bool has_value() { return state_ == VALUE; }
	bool has_exception() { return state_ == EXCEPTION; }
	
	void subscribe(closure_t subscriber) {
		bq_cond_t::handler_t handler(ready_cond_);

		if(state_ == EMPTY) {
			subscribers_.push_back(subscriber);
		} else {
			ready_cond_.unlock();
			subscriber();
			ready_cond_.lock();
		}	
	}

	friend class ref_t<shared_state_base_t>;

protected:	
	bq_cond_t ready_cond_;

	enum state_t {
		EMPTY, VALUE, EXCEPTION
	};

	std::atomic<int> state_;

	std::exception_ptr err_;
	std::vector<closure_t> subscribers_;

	void notify_subscribers() {
		std::vector<closure_t> subscribers;
		bq_cond_t::handler_t handler(ready_cond_);
		subscribers.swap(subscribers_);
		ready_cond_.unlock();

		for(const auto& subscriber : subscribers) {
			subscriber();
		}
	}
};

template<class x_t>
class shared_state_t : public shared_state_base_t {
public:
	const x_t& get() {
		wait(NULL);

		if(state_ == EXCEPTION) {
			std::rethrow_exception(err_);
		} else {
			return *value_;
		}
	}

	void set_value(const x_t& value) {
		bq_cond_t::handler_t handler(ready_cond_);

		assert(state_ == EMPTY);
		state_ = VALUE;
		value_.reset(new x_t(value));

		handler.send(true);
		ready_cond_.unlock();
		notify_subscribers();		
	}

	friend class future_t<x_t>;
	friend class promise_t<x_t>;
	friend class ref_t<shared_state_t>;

private:
	std::unique_ptr<x_t> value_;
};

template<>
class shared_state_t<void> : public shared_state_base_t {
private:
	void get() {
		wait(NULL);

		if(state_ == EXCEPTION) {
			std::rethrow_exception(err_);
		}
	}


	void set_value() {
		bq_cond_t::handler_t handler(ready_cond_);

		assert(state_ == EMPTY);
		state_ = VALUE;

		handler.send(true);
		ready_cond_.unlock();
		notify_subscribers();
	}

	friend class future_t<void>;
	friend class promise_t<void>;
	friend class ref_t<shared_state_t>;
};

template<class x_t>
class future_t {
private:
	future_t(ref_t<shared_state_t<x_t>> state) : state_(state) {}

public:
	future_t() {}
	future_t(future_t&& other) : state_(other.state_) {}
	future_t(const future_t& other) = default;

	future_t& operator = (future_t&& other) {
		state_ = other.state_;
		return *this;
	}

	future_t& operator = (const future_t& other) = default;

	// magic required for future_t<void>
	typedef typename std::add_lvalue_reference<typename std::add_const<x_t>::type>::type x_const_ref_t;

	x_const_ref_t get() {
		assert(state_);
		return state_->get();
	}

	std::exception_ptr get_exception() {
		assert(state_);
		return state_->get_exception();
	}

	bool is_ready() {
		assert(state_);
		return state_->is_ready();
	}

	bool is_valid() {
		return state_;
	}

	bool has_value() {
		assert(state_);
		return state_->has_value();
	}

	bool has_exception() {
		assert(state_);
		return state_->has_exception();
	}

	bool wait(interval_t* interval = NULL) {
		assert(state_);
		return state_->wait(interval);
	}

	template<class y_t>
	future_t<y_t> then(std::function<y_t(future_t<x_t>)> f) {
		assert(state_);
		promise_t<y_t> chained_promise;
		future_t<y_t> chained_future = chained_promise.get_future();

		state_->subscribe([f, chained_promise, state_] () mutable {
			future_t<x_t> future(state_);

			try {
				future_traits_t<y_t>::apply_and_set_value(&chained_promise, &future, &f);
			} catch(...) {
				chained_promise.set_exception(std::current_exception());
			}
		});

		return chained_future;
	}

	template<class y_t>
	future_t<y_t> then(std::function<future_t<y_t>(future_t<x_t>)> f) {
		assert(state_);

		promise_t<y_t> chained_promise;
		future_t<y_t> chained_future = chained_promise.get_future();

		std::function<void(future_t<y_t>)> outer_handler = [chained_promise] (future_t<y_t> future) mutable {
			if(future.has_exception()) {
				chained_promise.set_exception(future.get_exception());
			} else {
				future_traits_t<y_t>::forward_value(&chained_promise, &future);
			}
		};

		state_->subscribe([f, chained_promise, outer_handler, state_] () mutable {
			future_t<x_t> inner_future(state_);

			future_t<y_t> outer_future;
			try {
				outer_future = f(inner_future);
				outer_future.then(outer_handler);
			} catch(...) {
				chained_promise.set_exception(std::current_exception());
			}
		});

		return chained_future;
	}

	friend class promise_t<x_t>;

private:
	ref_t<shared_state_t<x_t>> state_;
};

template<class x_t>
class promise_t {
public:
	promise_t() : state_(new shared_state_t<x_t>()) {}
	promise_t(promise_t&& other) {
		state_ = other.state_;
	}
	promise_t(const promise_t& other) = default;

	promise_t& operator = (promise_t&& other) {
		state_ = other.state_;
		return *this;
	}
	promise_t& operator = (const promise_t& other) = default;

	future_t<x_t> get_future() {
		return future_t<x_t>(state_);
	}

	// magic required for future_t<void>
	template<class... args_t>
	void set_value(const args_t&... args) {
		state_->set_value(args...);
	}

	void set_exception(std::exception_ptr err) {
		state_->set_exception(err);
	}

private:
	ref_t<shared_state_t<x_t>> state_;
};

template<class x_t>
inline future_t<x_t> make_ready_future(const x_t& x) {
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
inline future_t<x_t> make_exception_future(std::exception_ptr err) {
	promise_t<x_t> promise;
	promise.set_exception(err);
	return promise.get_future();
}

template<class x_t>
struct future_traits_t {
	template<class y_t>
	static void apply_and_set_value(
		promise_t<x_t>* promise,
		future_t<y_t>* future,
		const std::function<x_t(future_t<y_t>)>* f
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
	template<class y_t>
	static void apply_and_set_value(
		promise_t<void>* promise,
		future_t<y_t>* future,
		const std::function<void(future_t<y_t>)>* f
	) {
		(*f)(*future);
		promise->set_value();
	}

	static void forward_value(
		promise_t<void>* promise,
		future_t<void>* future
	) {
		future->get();
		promise->set_value();
	}
};

} // namespace phantom
