#include <raptor/server/tcp_server.h>

#include <sys/socket.h>

#include <gmock/gmock.h>

using namespace raptor;

TEST(sockaddr, test) {
	struct sockaddr_storage sas;
}

TEST(tcp_server_test_t, simple_bind) {
	auto s = make_scheduler();

	tcp_server_t server(s.get(), nullptr, 9999);
}

TEST(tcp_server_test_t, bind_on_used_port) {
	auto s = make_scheduler();

	tcp_server_t server(s.get(), nullptr, 9999);
	ASSERT_THROW(tcp_server_t(s.get(), nullptr, 9999), std::runtime_error);
}
