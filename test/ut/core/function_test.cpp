#include <functional>
#include <memory>

#include <gtest/gtest.h>

TEST(function, delete_this) {
	std::shared_ptr<std::function<void()>> fn = std::make_shared<std::function<void()>>([&] () {
		fn.reset();
	});

	(*fn)();
}
