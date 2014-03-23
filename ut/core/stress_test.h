#pragma once

#include <gmock/gmock.h>

#include <memory>
#include <vector>
#include <thread>

#include <raptor/core/impl.h>

using namespace testing;
using namespace raptor;

struct stress_test_t : public Test {
	static const size_t N_THREADS = 4;

	virtual void SetUp() {
		for(size_t i = 0; i < N_THREADS; ++i) {
			schedulers.emplace_back(new scheduler_impl_t());
			auto scheduler = schedulers.back().get();
			threads.emplace_back([scheduler] () { scheduler->run(); });
		}
	}

	virtual void TearDown() {
		for(size_t i = 0; i < N_THREADS; ++i) {
			schedulers[i]->break_loop();
		}

		for(size_t i = 0; i < N_THREADS; ++i) {
			threads[i].join();
		}
	}

	std::vector<std::unique_ptr<scheduler_impl_t>> schedulers;
	std::vector<std::thread> threads;
};
