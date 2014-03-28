#include <memory>
#include <thread>

#include <gtest/gtest.h>

TEST(thread_lambda_dtor, destroys_captured_variables) {
	auto ptr = std::make_shared<int>(10);

	std::thread thread([ptr] () { *ptr += 1; });
	thread.join();

	EXPECT_TRUE(ptr.unique());
}
