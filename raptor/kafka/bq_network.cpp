#include <raptor/kafka/bq_network.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <string.h>

#include <pd/base/log.H>
#include <pd/base/exception.H>
#include <pd/bq/bq_util.H>
#include <pd/bq/bq_job.H>

#include <raptor/kafka/bq_link.h>
#include <raptor/kafka/request.h>
#include <raptor/kafka/response.h>

namespace raptor { namespace io_kafka {

fd_t connect(const std::string& host, uint16_t port) {
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
		throw sys_exception_t("getaddrinfo(): ", gai_strerror(res));
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
		throw sys_exception_t("phantom::io_kafka::connect(): %s", strerror(last_errno));
	} else {
		if(pd::bq_fd_setup(sock) < 0)
			throw sys_exception_t("bq_fd_setup(): ", strerror(errno));

		return fd_t(sock);
	}
}

std::shared_ptr<link_t> bq_network_t::make_link(const std::string& hostname, uint16_t port) {
	fd_t conn = connect(hostname, port);
	auto link = std::make_shared<bq_link_t>(std::move(conn), options);
	link->start(scheduler);

	return link;
}


bq_network_t::bq_network_t(scheduler_t* scheduler, const options_t& options) :
	scheduler(scheduler),
	is_refreshing(false),
	options(options) {}

void bq_network_t::do_refresh_metadata() {

	interval_t backoff = options.lib.metadata_refresh_backoff;
	bq_sleep(&backoff);

	try {
		bq_mutex_guard_t guard(mutex);
		metadata_t::addr_t broker_addr = metadata.get_next_broker();
		guard.relax();

		metadata_request_ptr_t request = std::make_shared<metadata_request_t>();
		metadata_response_ptr_t response = std::make_shared<metadata_response_t>();

		std::shared_ptr<link_t> meta_link = make_link(broker_addr.hostname, broker_addr.port);
		meta_link->send(request, response).get();

		guard.wakeup();
		metadata.update(*response);
	} catch(std::exception& err) {
		log_error("do_refresh_metadata(): %s", err.what());
	}

	bq_mutex_guard_t guard(mutex);
	is_refreshing = false;
}

void bq_network_t::add_broker(const std::string& host, uint16_t port) {
	bq_mutex_guard_t guard(mutex);
	metadata.add_broker(host, port);
}

void bq_network_t::refresh_metadata() {
	bq_mutex_guard_t guard(mutex);
	if(is_refreshing) return;
	is_refreshing = true;
	guard.relax();

	bq_job(&bq_network_t::do_refresh_metadata)(*this)->run(scheduler->bq_thr());
}

future_t<link_ptr_t> bq_network_t::get_link(const std::string& topic, partition_id_t partition) {
	bq_mutex_guard_t guard(mutex);

	try {
		int32_t broker_id = metadata.get_partition_leader(topic, partition);
		if(broker_id == -1)
			throw exception_t("leader not elected: " + topic + ":" + std::to_string(partition));

		std::shared_ptr<link_t>& broker_link = active_links[broker_id];

		if(!broker_link || broker_link->is_closed()) {
			metadata_t::addr_t addr = metadata.get_host_addr(broker_id);
			broker_link = make_link(addr.hostname, addr.port);
		}

		return make_ready_future(broker_link);
	} catch (const std::runtime_error& err) {
		return make_exception_future<link_ptr_t>(std::current_exception());
	}
}

}} // namespace raptor::io_kafka
