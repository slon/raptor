#include <raptor/core/scheduler.h>

#include <gtest/gtest.h>

using namespace raptor;

TEST(scheduler_test_t, create_shutdown) {
	auto s = make_scheduler();
	s->shutdown();
}

TEST(scheduler_test_t, fiber_destroys_captures_vars) {
	auto s = make_scheduler();

	auto ptr = std::make_shared<int>(1);
	s->start([ptr] () { *ptr += 1; }).join();

	s->shutdown();

	EXPECT_TRUE(ptr.unique());
}

TEST(scheduler_test_t, start_fiber) {
	bool runned = false;

	auto s = make_scheduler();

	fiber_t f = s->start([&runned] () {
		runned = true;
	});

	f.join();
	s->shutdown();

	EXPECT_EQ(true, runned);
}

__thread int v = 0;
int* v_ptr() { return &v; }

TEST(scheduler_test_t, switch_between) {
	auto s1 = make_scheduler(), s2 = make_scheduler();

	int v1, v2, v3;

	fiber_t f = s1->start([&] () {
		*v_ptr() = 1;

		s1->switch_to();
		v1 = *v_ptr();
		*v_ptr() = 2;
		s2->switch_to();
		v2 = *v_ptr();
		*v_ptr() = 3;
		s1->switch_to();
		v3 = *v_ptr();
	});

	f.join();
	s1->shutdown();
	s2->shutdown();

	EXPECT_EQ(1, v1);
	EXPECT_EQ(0, v2);
	EXPECT_EQ(2, v3);
}
