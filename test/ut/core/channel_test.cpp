#include <raptor/core/channel.h>

#include <atomic>

#include <gtest/gtest.h>

#include <raptor/core/scheduler.h>
#include <raptor/core/fiber.h>

#include "stress_test.h"

using namespace raptor;

struct ring_buffer_test_t : public ::testing::Test {
	ring_buffer_test_t() : ring(4) {}

	void test_put(int i, bool result, bool full, bool empty) {
		EXPECT_EQ(result, ring.try_put(i));
		EXPECT_EQ(full, ring.is_full());
		EXPECT_EQ(empty, ring.is_empty());
	}

	void test_get(int i, bool result, bool full, bool empty) {
		int v = -1;
		EXPECT_EQ(result, ring.try_get(&v));
		if(result) EXPECT_EQ(i, v);
		EXPECT_EQ(full, ring.is_full());
		EXPECT_EQ(empty, ring.is_empty());
	}

	ring_buffer_t<int> ring;
};

TEST_F(ring_buffer_test_t, ring_buffer) {
	EXPECT_TRUE(ring.is_empty());
	EXPECT_FALSE(ring.is_full());

	test_put(1, true, false, false);
	test_get(1, true, false, true);

	test_get(-1, false, false, true);

	test_put(2, true, false, false);
	test_put(3, true, false, false);
	test_put(4, true, false, false);
	test_put(5, true, true, false);

	test_put(6, false, true, false);

	test_get(2, true, false, false);
	test_get(3, true, false, false);
	test_get(4, true, false, false);
	test_get(5, true, false, true);

	test_get(-1, false, false, true);
}

TEST(channel_test_t, put_get_without_blocking) {
	channel_t<int> chan(1);
	EXPECT_TRUE(chan.put(10));
	int i = 0;
	EXPECT_TRUE(chan.get(&i));
	EXPECT_EQ(10, i);
}

TEST(channel_test_t, get_blocks) {
	channel_t<int> channel(1);
	scheduler_t sched;

	bool get_res;
	int get_val = -1;
	fiber_t fiber = sched.start([&] () {
		get_res = channel.get(&get_val);
	});

	usleep(10000);
	EXPECT_EQ(-1, get_val);

	channel.put(10);
	fiber.join();
	EXPECT_EQ(10, get_val);
	EXPECT_EQ(true, get_res);

	sched.shutdown();
}

TEST(channel_test_t, put_blocks) {
	channel_t<int> channel(3);
	EXPECT_TRUE(channel.put(1));
	EXPECT_TRUE(channel.put(2));
	EXPECT_TRUE(channel.put(3));

	scheduler_t sched;

	bool put_res;
	bool blocked = true;
	fiber_t fiber = sched.start([&] () {
		put_res = channel.put(4);
		blocked = false;
	});

	usleep(10000);
	EXPECT_TRUE(blocked);

	int i = -1;
	EXPECT_TRUE(channel.get(&i));
	EXPECT_EQ(1, i);

	fiber.join();
	sched.shutdown();

	EXPECT_FALSE(blocked);
	EXPECT_TRUE(put_res);

	EXPECT_TRUE(channel.get(&i));
	EXPECT_TRUE(channel.get(&i));
	EXPECT_TRUE(channel.get(&i));
	EXPECT_EQ(4, i);
}

TEST(channel_test_t, closed) {
	channel_t<int> channel(3);
	channel.put(1);
	channel.put(2);

	channel.close();

	EXPECT_FALSE(channel.put(3));

	int i = -1;
	EXPECT_TRUE(channel.get(&i));
	EXPECT_EQ(1, i);

	EXPECT_TRUE(channel.get(&i));
	EXPECT_EQ(2, i);

	EXPECT_FALSE(channel.get(&i));
}

TEST(channel_test_t, close_unblocks_reader) {
	channel_t<int> channel(1);
	scheduler_t sched;

	bool get_res;
	fiber_t fiber = sched.start([&] () {
		int i;
		get_res = channel.get(&i);
	});

	usleep(100);
	channel.close();
	fiber.join();
	sched.shutdown();

	EXPECT_FALSE(get_res);
}

TEST(channel_test_t, close_unblocks_writer) {
	channel_t<int> channel(1);
	scheduler_t sched;
	channel.put(1);

	bool put_res;
	fiber_t fiber = sched.start([&] () {
		put_res = channel.put(2);
	});

	usleep(10000);
	channel.close();
	fiber.join();
	sched.shutdown();

	EXPECT_FALSE(put_res);
}

TEST_F(stress_test_t, sum) {
	int N_FIBERS = 500;
	int N_INTS = 500;
	std::vector<fiber_t> readers, writers;

	channel_t<int> channel(100);

	std::atomic<bool> all_puts_ok(true);
	std::atomic<int> sum_ints(0);
	for(int i = 0; i < N_FIBERS; ++i) {
		writers.push_back(make_fiber([&] () {
			for(int i = 0; i < N_INTS; ++i) {
				sum_ints += i;
				bool res = channel.put(i);
				if(!res) all_puts_ok = false;
			}
		}));
	}

	std::atomic<int> sum_get_ints(0);
	for(int i = 0; i < N_FIBERS; ++i) {
		readers.push_back(make_fiber([&] () {
			int v;
			while(channel.get(&v)) {
				sum_get_ints += v;
			}
		}));
	}

	for(auto& f : writers) f.join();
	channel.close();
	for(auto& f: readers) f.join();

	EXPECT_TRUE(all_puts_ok);
	EXPECT_EQ(sum_ints, sum_get_ints);
}
