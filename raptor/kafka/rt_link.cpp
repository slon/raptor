#include <raptor/kafka/rt_link.h>

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <string.h>

#include <glog/logging.h>

#include <raptor/core/scheduler.h>
#include <raptor/core/syscall.h>

#include <raptor/kafka/options.h>
#include <raptor/kafka/request.h>
#include <raptor/kafka/response.h>

namespace raptor { namespace kafka {

fd_guard_t connect_socket(const std::string& host, uint16_t port) {
	std::string port_str = std::to_string(port);

	struct addrinfo hints;
	struct addrinfo* results;
	struct addrinfo* result;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_canonname = NULL;
	hints.ai_addr = NULL;
	hints.ai_next = NULL;

	int res;
	if((res = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &results)) != 0) {
		throw std::system_error(errno, std::system_category(), "getaddrinfo(" + host + ":" + port_str + ")");
	}

	int last_errno = 0;

	int sock = -1;
	for(result = results; result != NULL; result = result->ai_next) {
		sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		if(sock == -1) {
			last_errno = errno;
			continue;
		}

		if(connect(sock, result->ai_addr, result->ai_addrlen) != -1) {
			break;
		}

		last_errno = errno;
		close(sock);
		sock = -1;
	}

	freeaddrinfo(results);

	if(sock == -1) {
		throw std::system_error(last_errno, std::system_category(), "connect(" + host + ":" + port_str + ")");
	} else {
		if(rt_ctl_nonblock(sock) < 0)
			throw std::system_error(last_errno, std::system_category(), "rt_ctl_nonblock(" + host + ":" + port_str + ")");

		return fd_guard_t(sock);
	}
}

void recv_all(int fd, char* buff, size_t size) {
	size_t bytes_read = 0;
	while(bytes_read < size) {
		ssize_t n = rt_read(fd, buff + bytes_read, size - bytes_read, NULL);

		if(n == 0) {
			throw exception_t("bq_read(): connection closed");
		}

		if(n < 0) {
			throw std::system_error(errno, std::system_category(), "recv_all");
		}
		bytes_read += n;
	}
}

blob_t read_blob(int fd) {
	int32_t blob_size;

	recv_all(fd, reinterpret_cast<char*>(&blob_size), sizeof(int32_t));
	blob_size = be32toh(blob_size);

	if(blob_size > 64 * 1024 * 1024) {
		throw exception_t("blob size > 64MB");
	}

	std::shared_ptr<char> blob(new char[blob_size], [] (char* p) { delete[] p; });
	recv_all(fd, blob.get(), blob_size);

	return blob_t(blob, blob_size);
}

rt_link_t::rt_link_t(fd_guard_t socket, const options_t& options)
	: socket(std::move(socket)), options(options), send_channel(128), recv_channel(128) {}

rt_link_t::~rt_link_t() {
	close();

	if(recv_fiber.is_valid())
		recv_fiber.join();

	if(send_fiber.is_valid())
		send_fiber.join();
}

void rt_link_t::start(scheduler_t* scheduler) {
	send_fiber = scheduler->start(&rt_link_t::send_loop, this);
	recv_fiber = scheduler->start(&rt_link_t::recv_loop, this);
}

future_t<void> rt_link_t::send(request_ptr_t request, response_ptr_t response) {
	task_t task;
	task.request = request;
	task.response = response;

	future_t<void> completed = task.promise.get_future();

	if(!send_channel.put(task)) { // connection closed
		throw std::runtime_error("rt_link_t is closed #0");
	}

	return completed;
}

void rt_link_t::send_loop() {
	char obuf[options.lib.obuf_size];
	bq_wire_writer_t writer(socket.fd(), obuf, sizeof(obuf));

	task_t task;

	while(send_channel.get(&task)) {
		try {
			task.request->write(&writer);
			writer.flush_all();

			if(!task.response)
				task.promise.set_value();

			if(!recv_channel.put(task)) {
				task.promise.set_exception(std::make_exception_ptr(std::runtime_error("rt_link_t is closed #1")));
			}
		} catch (...) {
			task.promise.set_exception(std::current_exception());
			break;
		}
	}

	close();

	std::exception_ptr err = std::make_exception_ptr(std::runtime_error("rt_link_t is closed #2"));
	while(send_channel.get(&task)) {
		task.promise.set_exception(err);
	}
}

void rt_link_t::recv_loop() {
	task_t task;

	while(recv_channel.get(&task)) {
		try {
			blob_t blob = read_blob(socket.fd());
			wire_reader_t reader(blob);
			task.response->read(&reader);
			task.promise.set_value();
		} catch(...) {
			task.promise.set_exception(std::current_exception());
			break;
		}
	}

	close();

	std::exception_ptr err = std::make_exception_ptr(std::runtime_error("rt_link_t is closed #3"));
	while(recv_channel.get(&task)) {
		task.promise.set_exception(err);
	}
}

void rt_link_t::close() {
	send_channel.close();
	recv_channel.close();
}

bool rt_link_t::is_closed() {
	return send_channel.is_closed();
}

}} // namespace raptor::kafka
