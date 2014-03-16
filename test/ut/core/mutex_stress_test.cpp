#include <raptor/core/mutex.h>

#include "stress_test.h"

struct mutex_stress_test_t : public stress_test_t {
	void run_test(int n_threads, bool jump_around) {
		const int N_FIBERS = 1000, N_INCS = 1000;

		std::vector<std::unique_ptr<fiber_impl_t>> fibers;

		mutex_t mutex;
		int value = 0;

		auto lock_inc_unlock = [&] () {
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
			fibers.emplace_back(new fiber_impl_t(lock_inc_unlock));

			schedulers[i % n_threads]->activate(fibers.back().get());
		}

		// busy wait for termination
		for(int i = 0; i < N_FIBERS; ++i) {
			while(!fibers[i]->is_terminated()) usleep(10000);
		}

		EXPECT_EQ(N_FIBERS * N_INCS, value);
	}
};

TEST_F(mutex_stress_test_t, one_thread) {
	run_test(1, false);
}

TEST_F(mutex_stress_test_t, many_threads) {
	run_test(schedulers.size(), false);
}

TEST_F(mutex_stress_test_t, many_threads_with_jumping_fibers) {
	run_test(schedulers.size(), true);
}
