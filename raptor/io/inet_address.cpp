#include <raptor/io/inet_address.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdexcept>

namespace raptor {

static inet_address_t parse_inet_address(const std::string& ip, const std::string& port) {
	struct addrinfo hints;
	struct addrinfo* result;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST;

	inet_address_t address;

	int res = 0;
	if((res = getaddrinfo(ip.c_str(), port.empty() ? nullptr : port.c_str(), &hints, &result)) == 0) {
		if(result->ai_family == AF_INET || result->ai_family == AF_INET6) {
			memcpy(&address.ss, result->ai_addr, result->ai_addrlen);
			address.ss_len = result->ai_addrlen;
		} else {
			freeaddrinfo(result);
			throw std::runtime_error("unknown ai_family for ip address '" + ip +
				"': " + std::to_string(result->ai_family));
		}

		freeaddrinfo(result);
	} else {
		throw std::runtime_error("invalid ip address '" + ip + "' :" + gai_strerror(res));
	}

	return address;
}

inet_address_t inet_address_t::parse_ip(const std::string& ip) {
	return parse_inet_address(ip, "");
}

inet_address_t inet_address_t::parse_ip_port(const std::string& ip, const std::string& port) {
	return parse_inet_address(ip, port);
}

} // namespace raptor
