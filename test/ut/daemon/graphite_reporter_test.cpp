#include <raptor/daemon/graphite_reporter.h>

#include <gtest/gtest.h>

#include <pm/metrics.h>

using namespace raptor;

TEST(graphite_reporter_test_t, test) {
	pm::counter_t c = pm::get_root().counter("counter");

	scheduler_t s;
	graphite_reporter_t reporter(&s);

	usleep(10000);
}
