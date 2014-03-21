#include <raptor/core/future.h>

#include <raptor/core/scheduler.h>
#include <raptor/core/fiber.h>

#include <gtest/gtest.h>

using namespace raptor;

TEST(future_test_t, empty_state) {
	future_t<void> f;
	EXPECT_FALSE(f.is_valid());
}

TEST(future_test_t, make_ready_future) {
	future_t<void> ready = make_ready_future();
	EXPECT_TRUE(ready.has_value());

	future_t<int> ready_int = make_ready_future(5);
	EXPECT_EQ(5, ready_int.get());
}

TEST(future_test_t, make_exception_future) {
	future_t<void> f = make_exception_future<void>(std::runtime_error("bad thing happend"));
	EXPECT_TRUE(f.has_exception());
	EXPECT_THROW(f.get(), std::runtime_error);	

	future_t<void> f2 = make_exception_future<void>(std::make_exception_ptr(std::runtime_error("")));
	EXPECT_TRUE(f2.has_exception());
	EXPECT_THROW(f2.get(), std::runtime_error);
}

TEST(future_test_t, future_value_block) {
	scheduler_t s;

	promise_t<int> p;
	future_t<int> fu = p.get_future();

	int res = -1;
	fiber_t f = s.start([&] () {
		res = fu.get();
	});

	usleep(10000);
	EXPECT_EQ(-1, res);
	EXPECT_FALSE(fu.is_ready());
	EXPECT_FALSE(fu.has_value());
	EXPECT_FALSE(fu.has_exception());

	p.set_value(1);
	f.join();
	s.shutdown();

	EXPECT_TRUE(fu.is_ready());
	EXPECT_TRUE(fu.has_value());
	EXPECT_FALSE(fu.has_exception());

	EXPECT_EQ(1, res);
}

TEST(future_test_t, future_exception_block) {
	scheduler_t s;

	promise_t<int> p;
	future_t<int> fu = p.get_future();

	bool exception = false;
	fiber_t f = s.start([&] () {
		try {
			fu.get();
		} catch(const std::runtime_error&) {
			exception = true;
		}
	});

	usleep(10000);
	EXPECT_FALSE(exception);
	EXPECT_FALSE(fu.is_ready());
	EXPECT_FALSE(fu.has_value());
	EXPECT_FALSE(fu.has_exception());

	p.set_exception(std::make_exception_ptr(std::runtime_error("very bad")));
	f.join();
	s.shutdown();

	EXPECT_TRUE(fu.is_ready());
	EXPECT_FALSE(fu.has_value());
	EXPECT_TRUE(fu.has_exception());

	EXPECT_TRUE(exception);
}

TEST(future_test_t, future_wait_timeout) {
	promise_t<int> broken_promise;
	future_t<int> future = broken_promise.get_future();

	duration_t timeout(0.01);
	EXPECT_FALSE(future.wait(&timeout));
	EXPECT_GE(duration_t(0.0), timeout);
}

TEST(future_test_t, then_on_ready_future) {
	future_t<int> future = make_ready_future(1);
	future_t<double> next_future = future.then([] (future_t<int> x) { return x.get() + 0.0; });

	EXPECT_TRUE(next_future.has_value());
	EXPECT_DOUBLE_EQ(1.0, next_future.get());

	next_future = future.then([] (future_t<int>) -> double { throw std::runtime_error(""); });
	EXPECT_TRUE(next_future.has_exception());
	EXPECT_THROW(next_future.get(), std::runtime_error);
}

TEST(future_test_t, then_on_exception_future) {
	future_t<int> future = make_exception_future<int>(std::runtime_error(""));

	future_t<double> next_future = future.then([] (future_t<int> f) { return f.get() + 1.0; });
	EXPECT_TRUE(next_future.has_exception());
	EXPECT_THROW(next_future.get(), std::runtime_error);
}

TEST(future_test_t, then_silence_exception) {
	future_t<int> future = make_exception_future<int>(std::runtime_error(""));

	future_t<int> future2 = future.then([] (future_t<int> f) { return 2; });
	EXPECT_TRUE(future2.has_value());
	EXPECT_EQ(2, future2.get());
}

TEST(future_test_t, then_on_blocking_future) {
	promise_t<int> p;
	future_t<int> f = p.get_future();

	future_t<double> f2 = f.then([] (future_t<int> x) { return x.get() + 1.0; });
	future_t<void> f3 = f.then([] (future_t<int>) -> void { throw std::runtime_error(""); });

	EXPECT_FALSE(f2.is_ready());
	EXPECT_FALSE(f3.is_ready());

	p.set_value(1);

	EXPECT_TRUE(f2.has_value());
	EXPECT_DOUBLE_EQ(2.0, f2.get());

	EXPECT_TRUE(f3.has_exception());
	EXPECT_THROW(f3.get(), std::runtime_error);
}

TEST(future_test_t, then_on_exception_blocking_future) {
	promise_t<int> p;
	future_t<int> f = p.get_future();

	future_t<void> f2 = f.then([] (const future_t<int>& f) { f.get(); });
	EXPECT_FALSE(f2.is_ready());

	p.set_exception(std::make_exception_ptr(std::runtime_error("")));
}
