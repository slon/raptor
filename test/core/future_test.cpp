#include <raptor/core/future.h>

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
