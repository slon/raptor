#pragma once

#include <gmock/gmock.h>

#include <memory>
#include <atomic>
#include <vector>
#include <thread>

#include <raptor/core/scheduler.h>

using namespace testing;
using namespace raptor;

struct stress_test_t : public Test {
	static const size_t N_THREADS = 4;

	std::vector<scheduler_ptr_t> schedulers;
	std::atomic<int> next_scheduler;

	virtual void SetUp() {
		for(size_t i = 0; i < N_THREADS; ++i) {
			schedulers.push_back(make_scheduler("s" + std::to_string(i)));
		}
	}

	virtual void TearDown() {
		for(auto& s : schedulers) s->shutdown();
	}


	fiber_t make_fiber(std::function<void()> c) {
		return schedulers[next_scheduler++ % schedulers.size()]->start(c);
	}
};
