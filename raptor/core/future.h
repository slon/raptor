#pragma once

#include <vector>
#include <memory>
#include <type_traits>

#include <raptor/core/wait_queue.h>
#include <raptor/core/executor.h>

namespace raptor {

template<class x_t> class future_t;
template<class x_t> class promise_t;
template<class x_t> class shared_state_t;

// make future from value
template<class x_t>
future_t<x_t> make_ready_future(const x_t& x);
future_t<void> make_ready_future();

// make future from exception
template<class x_t, class exception_t>
future_t<x_t> make_exception_future(const exception_t& err);
template<class x_t>
future_t<x_t> make_exception_future(std::exception_ptr err);

// read-only result of asyncronous operation or exception describing why operation failed
template<class x_t>
class future_t {
public:
	// create future with empty state
	future_t() {}

	typedef x_t value_t;

	// just 'const x_t&', than compiles when x_t is void
	typedef typename std::add_lvalue_reference<typename std::add_const<x_t>::type>::type x_const_ref_t;

	// block untill future is ready and return value or throw exception
	x_const_ref_t get() const;

	// block untill future is ready and return exception
	std::exception_ptr get_exception() const;

	// is_valid() == false for default constructed future
	// is_valid() == true of future obtained from promise.get_future
	// or future.then()
	bool is_valid() const;

	// should be obvious?
	bool is_ready() const;
	bool has_value() const;
	bool has_exception() const;

	// wait untill future is ready or timeout occur
	bool wait(duration_t* timeout = nullptr) const;

	// non-blocking equivalent to make_ready_future(fn(*this))
	template<class fn_t>
	auto then(fn_t&& fn) -> future_t<decltype(fn(future_t<x_t>()))> const;
	template<class fn_t>
	auto then(executor_t* executor, fn_t&& fn) -> future_t<decltype(fn(future_t<x_t>()))> const;

	// non-blocking equivalent to this->then(fn).get()
	template<class fn_t>
	auto bind(fn_t&& fn) -> decltype(fn(future_t<x_t>())) const;
	template<class fn_t>
	auto bind(executor_t* executor, fn_t&& fn) -> decltype(fn(future_t<x_t>())) const;

	friend class promise_t<x_t>;

private:
	explicit future_t(std::shared_ptr<shared_state_t<x_t>> state) : state_(state) {}

	std::shared_ptr<shared_state_t<x_t>> state_;
};

template<class x_t>
class promise_t {
public:
	promise_t();

	// get future associated with this promise
	future_t<x_t> get_future() const;

	// set value of the associated future, void version
	void set_value();

	// set value of the associated future, non void version
	template<class y_t>
	void set_value(const y_t& value);

	// set exception of the associated future
	void set_exception(std::exception_ptr err);

private:
	std::shared_ptr<shared_state_t<x_t>> state_;
};

} // namespace raptor

#include <raptor/core/future-inl.h>
