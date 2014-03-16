#pragma once

#include <gmock/gmock.h>

#include <thread>

#include <raptor/core/impl.h>

using namespace testing;
using namespace raptor;

struct running_scheduler_impl_test_t : public Test {
	virtual void SetUp() {
		t1 = std::thread([this] () { scheduler1.run(); });
		t2 = std::thread([this] () { scheduler2.run(); });
	}

	virtual void TearDown() {
		scheduler1.break_loop();
		scheduler2.break_loop();

		t1.join();
		t2.join();
	}

	scheduler_impl_t scheduler1;
	scheduler_impl_t scheduler2;
	std::thread t1, t2;
};
