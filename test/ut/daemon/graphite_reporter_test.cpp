#include <raptor/daemon/graphite_reporter.h>

#include <gtest/gtest.h>

#include <pm/metrics.h>

using namespace raptor;

TEST(graphite_reporter_test_t, test) {
	pm::counter_t c = pm::get_root().counter("counter");

	auto s = make_scheduler();

	graphite_reporter_t reporter(s);

	usleep(10000);
}
