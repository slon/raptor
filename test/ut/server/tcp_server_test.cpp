#include <raptor/server/tcp_server.h>

#include <sys/socket.h>

#include <gmock/gmock.h>

using namespace raptor;

TEST(sockaddr, test) {
	struct sockaddr_storage sas;
}

TEST(tcp_server_test_t, simple_bind) {
	scheduler_t scheduler;

	tcp_server_t server(&scheduler, nullptr, 9999);
}

TEST(tcp_server_test_t, bind_on_used_port) {
	scheduler_t scheduler;

	tcp_server_t server(&scheduler, nullptr, 9999);
	ASSERT_THROW(tcp_server_t(&scheduler, nullptr, 9999), std::runtime_error);
}
