#include <raptor/daemon/graphite_reporter.h>

#include <gtest/gtest.h>

using namespace raptor;

TEST(graphite_reporter_test_t, test) {
	scheduler_t s;
	graphite_reporter_t reporter(&s);

	usleep(100);
}
