#include <raptor/core/mutex.h>
#include <raptor/core/fiber.h>

#include "stress_test.h"

struct mutex_stress_test_t : public stress_test_t {
	void run_test(int n_threads, bool jump_around, bool with_native) {
		const int N_FIBERS = 500, N_INCS = 500, N_THREADS=4, N_THREAD_INCS = 50000;

		std::vector<fiber_t> fibers;
		std::vector<std::thread> threads;

		mutex_t mutex;
		int value = 0;

		std::function<void()> lock_inc_unlock = [&] () {
			for(int i = 0; i < N_INCS; ++i) {
				int old_value;

				std::unique_lock<mutex_t> guard(mutex);
				old_value = ++value;
				guard.unlock();

				if(jump_around)
					schedulers[old_value % schedulers.size()]->switch_to();
			}
		};

		for(int i = 0; i < N_FIBERS; ++i) {
			fibers.push_back(schedulers[i % n_threads]->start(lock_inc_unlock));
		}

		if(with_native) {
			for(int i = 0; i < N_THREADS; ++i) {
				threads.emplace_back([&] () {
					for(int i = 0; i < N_THREAD_INCS; ++i) {
						std::unique_lock<mutex_t> guard(mutex);
						++value;
					}
				});
			}
		}

		for(size_t i = 0; i < fibers.size(); ++i) {
			fibers[i].join();
		}

		for(size_t i = 0; i < threads.size(); ++i) {
			threads[i].join();
		}

		if(!with_native) {
			EXPECT_EQ(N_FIBERS * N_INCS, value);
		} else {
			EXPECT_EQ(N_FIBERS * N_INCS + N_THREADS * N_THREAD_INCS, value);
		}
	}
};

TEST_F(mutex_stress_test_t, one_thread) {
	run_test(1, false, false);
}

TEST_F(mutex_stress_test_t, many_threads) {
	run_test(schedulers.size(), false, false);
}

TEST_F(mutex_stress_test_t, many_threads_with_jumping_fibers) {
	run_test(schedulers.size(), true, false);
}

TEST_F(mutex_stress_test_t, many_threads_with_jumping_fibers_with_native) {
	run_test(schedulers.size(), true, true);
}
