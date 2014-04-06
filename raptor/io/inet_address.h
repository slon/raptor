#pragma once

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <string>

namespace raptor {

struct inet_address_t {
public:
	socklen_t ss_len;
	struct sockaddr_storage ss;

	static inet_address_t parse(const std::string& address);
	static inet_address_t parse_ip(const std::string& ip);
	static inet_address_t parse_ip_port(const std::string& ip, const std::string& port);

	std::string to_string();

	bool is_ipv4() {
		return ss.ss_family == AF_INET;
	}

	bool is_ipv6() {
		return ss.ss_family == AF_INET6;
	}

	struct sockaddr* addr() {
		return (struct sockaddr*)&ss;
	}

	socklen_t addrlen() {
		return ss_len;
	}

	socklen_t* addrlen_ptr() {
		return &ss_len;
	}

	uint16_t port() {
		switch(ss.ss_family) {
		case AF_INET:
			return ntohs(((struct sockaddr_in*)&ss)->sin_port);
		case AF_INET6:
			return ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
		default:
			return 0;
		}
	}

	inet_address_t() {
		ss_len = 0;
		memset(&ss, 0, sizeof(ss));
	}
};

} // namespace raptor
