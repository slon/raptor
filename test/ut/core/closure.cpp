#include <raptor/core/closure.h>

#include <memory>

#include <gtest/gtest.h>

using namespace raptor;

TEST(closure_t, delete_this) {
	std::shared_ptr<closure_t> closure = std::make_shared<closure_t>([&] () {
		closure.reset();
	});

	(*closure)();
}
