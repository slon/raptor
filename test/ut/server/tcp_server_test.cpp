#include <raptor/server/tcp_server.h>

#include <sys/socket.h>

#include <gmock/gmock.h>

#include <raptor/io/util.h>
#include <raptor/io/inet_address.h>

using namespace raptor;

TEST(tcp_server_test_t, simple_bind) {
	auto s = make_scheduler();

	tcp_server_t server(s, nullptr, 9999);
	server.shutdown();
}

TEST(tcp_server_test_t, bind_on_used_port) {
	auto s = make_scheduler();

	tcp_server_t server(s, nullptr, 9999);
	ASSERT_THROW(tcp_server_t(s, nullptr, 9999), std::runtime_error);

	server.shutdown();
}

struct test_handler_t : public tcp_handler_t {
	int size;

	virtual void on_accept(int fd) {
		std::string response(size, 'c');
		duration_t timeout(0.1);
		write_all(fd, response.data(), response.size(), &timeout);
	}
};

TEST(tcp_server_test_t, write_in_accept) {
	auto s = make_scheduler();

	auto handler = std::make_shared<test_handler_t>();

	tcp_server_t server(s, handler, 9998);

	auto addr = inet_address_t::resolve_ip_port("localhost", "9998");

	handler->size = 100000;
	auto fd = addr.connect(nullptr);

	usleep(10000);

	handler->size = 10000000;
	auto fd2 = addr.connect(nullptr);

	usleep(10000);

	server.shutdown();
}
