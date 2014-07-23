#include <raptor/io/inet_address.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <stdexcept>
#include <system_error>

#include <raptor/core/syscall.h>

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

static inet_address_t getaddrinfo(char const* hostname, char const* port, int ai_flags) {
	struct addrinfo hints;
	struct addrinfo* result;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = ai_flags;

	inet_address_t address;

	int res = 0;
	if((res = ::getaddrinfo(hostname, port, &hints, &result)) == 0) {
		if(result->ai_family == AF_INET || result->ai_family == AF_INET6) {
			memcpy(&address.ss, result->ai_addr, result->ai_addrlen);
			address.ss_len = result->ai_addrlen;
		} else {
			freeaddrinfo(result);
			throw std::runtime_error("unknown ai_family");
		}

		freeaddrinfo(result);
	} else {
		throw std::runtime_error("getaddrinfo(): '" + std::string(hostname) + "': " + gai_strerror(res));
	}

	return address;
}

inet_address_t inet_address_t::resolve_ip(const std::string& ip) {
	return getaddrinfo(ip.c_str(), NULL, 0);
}

inet_address_t inet_address_t::resolve_ip_port(const std::string& ip, const std::string& port) {
	return getaddrinfo(ip.c_str(), port.c_str(), 0);
}

inet_address_t inet_address_t::parse_ip(const std::string& ip) {
	return getaddrinfo(ip.c_str(), NULL, AI_NUMERICHOST);
}

inet_address_t inet_address_t::parse_ip_port(const std::string& ip, const std::string& port) {
	return getaddrinfo(ip.c_str(), port.c_str(), AI_NUMERICHOST);
}

fd_guard_t inet_address_t::bind() {
	fd_guard_t sock(socket(ss.ss_family, SOCK_STREAM, 0));

	if(sock.fd() < 0)
		throw std::system_error(errno, std::system_category(), "socket()");

	rt_ctl_nonblock(sock.fd());

	int i = 1;
	if(setsockopt(sock.fd(), SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) < 0)
		throw std::system_error(errno, std::system_category(), "setsockopt()");

	if(::bind(sock.fd(), addr(), addrlen()) < 0)
		throw std::system_error(errno, std::system_category(), "bind()");

	if(listen(sock.fd(), SOMAXCONN) < 0)
		throw std::system_error(errno, std::system_category(), "listen()");

	return std::move(sock);
}

fd_guard_t inet_address_t::connect(duration_t* timeout) {
	fd_guard_t sock(socket(ss.ss_family, SOCK_STREAM, 0));

	if(sock.fd() == -1) {
		throw std::system_error(errno, std::system_category(), "socket()");
	}

	rt_ctl_nonblock(sock.fd());

	if(rt_connect(sock.fd(), addr(), addrlen(), timeout) == -1) {
		throw std::system_error(errno, std::system_category(), "connect()");
	}

	return sock;
}

std::string inet_address_t::to_string() {
	std::string buffer;
	buffer.resize(128);

	if(inet_ntop(ss.ss_family, addr(), &buffer[0], buffer.size()) == 0) {
		throw std::system_error(errno, std::system_category(), "inet_ntop()");
	}

	buffer.resize(strlen(buffer.c_str()));
	return buffer + ":" + std::to_string(port());
}

} // namespace raptor
