#pragma once

#include <vector>
#include <memory>
#include <type_traits>

#include <raptor/core/wait_queue.h>
#include <raptor/core/closure.h>

namespace raptor {

template<class x_t> class future_t;
template<class x_t> class promise_t;

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

	void subscribe(closure_t&& cb) {
		std::unique_lock<spinlock_t> guard(lock_);

		if(state_ == EMPTY) {
			subscribers_.emplace_back(cb);
		} else {
			guard.unlock();
			cb();
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

		for(const auto& cb : subscribers) {
			cb();
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
class future_t {
private:
	explicit future_t(std::shared_ptr<shared_state_t<x_t>> state) : state_(state) {}

public:
	future_t() {}

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
		return state_ != nullptr;
	}

	bool has_value() {
		assert(state_);
		return state_->has_value();
	}

	bool has_exception() {
		assert(state_);
		return state_->has_exception();
	}

	bool wait(duration_t* timeout = nullptr) {
		assert(state_);
		return state_->wait(timeout);
	}

	friend class promise_t<x_t>;

private:
	std::shared_ptr<shared_state_t<x_t>> state_;
};

template<class x_t>
class promise_t {
public:
	promise_t() : state_(std::make_shared<shared_state_t<x_t>>()) {}

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
	std::shared_ptr<shared_state_t<x_t>> state_;
};

template<class x_t>
future_t<x_t> make_ready_future(const x_t& x) {
	promise_t<x_t> promise;
	promise.set_value(x);
	return promise.get_future();
}

future_t<void> make_ready_future() {
	promise_t<void> promise;
	promise.set_value();
	return promise.get_future();
}

template<class x_t, class exception_t>
future_t<x_t> make_exception_future(const exception_t& err) {
	promise_t<x_t> promise;
	promise.set_exception(std::make_exception_ptr(err));
	return promise.get_future();
}

template<class x_t>
future_t<x_t> make_exception_future(std::exception_ptr err) {
	promise_t<x_t> promise;
	promise.set_exception(err);
	return promise.get_future();
}

} // namespace raptor
