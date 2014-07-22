#include <raptor/io/inet_address.h>

#include <gtest/gtest.h>

#include <stdexcept>

#include <raptor/core/scheduler.h>

using namespace raptor;

TEST(inet_address_test_t, get_hostname) {
	ASSERT_FALSE(get_hostname().empty());
}

TEST(inet_address_test_t, get_fqdn) {
	ASSERT_FALSE(get_fqdn().empty());
}

TEST(inet_address_test_t, resolve) {
	inet_address_t addr = inet_address_t::resolve_ip("yandex.ru");
	ASSERT_THROW(inet_address_t::resolve_ip("asldkjfqwkjnflsakdjnv.rasdlu"), std::runtime_error);
}

TEST(inet_address_test_t, parse_ipv4) {
	inet_address_t empty;
	ASSERT_FALSE(empty.is_ipv4());
	ASSERT_FALSE(empty.is_ipv6());

	inet_address_t v4 = inet_address_t::parse_ip("1.2.3.4");

	ASSERT_TRUE(v4.is_ipv4());
	ASSERT_FALSE(v4.is_ipv6());
	ASSERT_EQ(0, v4.port());
	ASSERT_EQ(sizeof(sockaddr_in), v4.ss_len);

	struct in_addr inaddr;
	inaddr.s_addr = htonl((1 << 24) + (2 << 16) + (3 << 8) + 4);
	ASSERT_EQ(inaddr.s_addr, ((struct sockaddr_in*)(&v4.ss))->sin_addr.s_addr);
}

TEST(inet_address_test_t, parse_ipv6) {
	inet_address_t v6 = inet_address_t::parse_ip("fe80::1234");

	ASSERT_FALSE(v6.is_ipv4());
	ASSERT_TRUE(v6.is_ipv6());
	ASSERT_EQ(0, v6.port());
	ASSERT_EQ(sizeof(sockaddr_in6), v6.ss_len);

	struct in6_addr in6addr;
	memset(&in6addr, 0, sizeof(in6addr));
	in6addr.s6_addr[0] = 0xfe;
	in6addr.s6_addr[1] = 0x80;
	in6addr.s6_addr[14] = 0x12;
	in6addr.s6_addr[15] = 0x34;

	ASSERT_EQ(0, memcmp(&in6addr, &(((struct sockaddr_in6*)(&v6.ss))->sin6_addr), sizeof(in6addr)));
}

TEST(inet_address_test_t, parse_ip_port) {
	inet_address_t v4 = inet_address_t::parse_ip_port("1.2.3.4", "8080");
	ASSERT_EQ(8080, v4.port());

	inet_address_t v6 = inet_address_t::parse_ip_port("fe80::1234", "8080");
	ASSERT_EQ(8080, v6.port());
}

TEST(inet_address_test_t, parse_invalid_input) {
	ASSERT_THROW(inet_address_t::parse_ip(""), std::runtime_error);
	ASSERT_THROW(inet_address_t::parse_ip("yandex.ru"), std::runtime_error);
	ASSERT_THROW(inet_address_t::parse_ip("1.2.3.4.5"), std::runtime_error);
}

TEST(socket_test_t, normal_connect) {
	auto address = inet_address_t::parse_ip_port("127.0.0.1", "29854");

	auto server_socket = address.bind();

	auto client_socket1 = address.connect(nullptr);

	duration_t timeout(0.1);
	auto client_socket2 = address.connect(&timeout);
}

TEST(socket_test_t, connect_timeout) {
	auto s = make_scheduler();

	s->start([] () {
		auto address = inet_address_t::parse_ip_port("127.0.0.1", "29853");

		auto listening_socket = address.bind();
		try {
			while(true) {
				duration_t timeout(0.01);
				address.connect(&timeout);
			}
		} catch(std::exception& e) {
			ASSERT_TRUE(true);
		}
	}).join();
}
