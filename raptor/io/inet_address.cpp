#include <raptor/io/inet_address.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdexcept>
#include <system_error>

namespace raptor {

std::string get_hostname() {
	char hostname[1024 + 1];
	hostname[1024] = '\0';

	if(-1 == gethostname(hostname, 1024)) {
		throw std::system_error(errno, std::system_category(), "gethostname");
	}

	return std::string(hostname);
}

std::string get_fqdn() {
	std::string hostname = get_hostname();

	struct addrinfo hints, *info;
	int gai_result;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;

	if ((gai_result = getaddrinfo(hostname.c_str(), NULL, &hints, &info)) != 0) {
		throw std::runtime_error("getaddrinfo(" + hostname + "):" + gai_strerror(gai_result));
	}

	std::string fqdn(info->ai_canonname);

	freeaddrinfo(info);

	return fqdn;
}

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
