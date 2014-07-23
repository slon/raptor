#include "common.h"

#include <thread>

#include <raptor/server/tcp_server.h>

class kafka_link_test_t : public kafka_test_t {
	void SetUp() {
		kafka_test_t::SetUp();
		options.lib.link_timeout = std::chrono::milliseconds(10);
	}
};

TEST_F(kafka_link_test_t, failed_connection) {
	rt_kafka_link_t link({ "yandex.ru", 23451 }, scheduler, options);

	std::this_thread::sleep_for(options.lib.link_timeout * 5);

	ASSERT_TRUE(link.is_closed());
	link.shutdown();
}
