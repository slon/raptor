#include <raptor/kafka/kafka_cluster.h>

#include <raptor/io/util.h>
#include <raptor/io/inet_address.h>

#include <glog/logging.h>

namespace raptor { namespace kafka {

std::unique_ptr<io_buff_t> read_to_buff(int fd, duration_t* timeout) {
	int32_t buff_size;

	read_all(fd, reinterpret_cast<char*>(&buff_size), sizeof(int32_t), timeout);
	buff_size = be32toh(buff_size);

	if(buff_size > 64 * 1024 * 1024) {
		throw exception_t("blob size > 64MB");
	}

	auto buff = io_buff_t::create(buff_size);
	read_all(fd, (char*)buff->data(), buff_size, timeout);
	buff->append(buff_size);

	return std::move(buff);
}

rt_kafka_link_t::rt_kafka_link_t(
			const broker_addr_t& broker,
			scheduler_ptr_t scheduler,
			options_t options) :
		options_(options),
		send_channel_(4096),
		recv_channel_(4096) {
	recv_fiber_ = scheduler->start(&rt_kafka_link_t::recv_loop, this);
	send_fiber_ = scheduler->start(&rt_kafka_link_t::send_loop, this, broker);
}

void rt_kafka_link_t::connect(const broker_addr_t& broker) {
	inet_address_t broker_addr = inet_address_t::resolve_ip(broker.first);
	broker_addr.set_port(broker.second);

	duration_t timeout = options_.lib.link_timeout;
	socket_ = broker_addr.connect(&timeout);
}

void rt_kafka_link_t::send_loop(broker_addr_t broker) {
	kafka_rpc_t rpc;

	try {
		connect(broker);
	} catch(const std::exception& e) {
		close(std::current_exception());
	}

	while(send_channel_.get(&rpc)) {
		try {
			duration_t timeout = options_.lib.link_timeout;
			auto buf = rpc.request->serialize();
			write_all(socket_.fd(), buf.get(), &timeout);

			if(!rpc.response) {
				rpc.promise.set_value();
				continue;
			}

			if(!recv_channel_.put(rpc)) {
				rpc.promise.set_exception(get_closing_error());
			}
		} catch (const std::exception& e) {
			auto err = std::current_exception();
			close(err);
			rpc.promise.set_exception(get_closing_error());
		}
	}

	std::exception_ptr err = get_closing_error();
	while(send_channel_.get(&rpc)) {
		rpc.promise.set_exception(err);
	}
}

void rt_kafka_link_t::recv_loop() {
	kafka_rpc_t rpc;

	while(recv_channel_.get(&rpc)) {
		try {
			duration_t timeout = options_.lib.link_timeout;
			std::unique_ptr<io_buff_t> buff = read_to_buff(socket_.fd(), &timeout);
			wire_cursor_t cursor(buff.get());
			rpc.response->read(&cursor);
			rpc.promise.set_value();
		} catch(const std::exception& e) {
			auto err = std::current_exception();
			close(err);
			rpc.promise.set_exception(get_closing_error());
		}
	}

	std::exception_ptr err = get_closing_error();
	while(recv_channel_.get(&rpc)) {
		rpc.promise.set_exception(err);
	}
}

void rt_kafka_link_t::send(kafka_rpc_t rpc) {
	if(!send_channel_.put(rpc)) {
		rpc.promise.set_exception(get_closing_error());
	}
}

std::exception_ptr rt_kafka_link_t::get_closing_error() {
	std::unique_lock<mutex_t> guard(closing_error_lock_);
	return closing_error_;
}

void rt_kafka_link_t::close(std::exception_ptr err) {
	std::unique_lock<mutex_t> guard(closing_error_lock_);
	if(!closing_error_) closing_error_ = err;
	guard.unlock();

	send_channel_.close();
	recv_channel_.close();
}

bool rt_kafka_link_t::is_closed() {
	return send_channel_.is_closed();
}

void rt_kafka_link_t::shutdown() {
	close(std::make_exception_ptr(std::runtime_error("rt_kafka_link_t shutdown")));

	recv_fiber_.join();
	send_fiber_.join();
}

rt_kafka_network_t::rt_kafka_network_t(scheduler_ptr_t scheduler, const options_t& options) :
	scheduler_(scheduler),
	options_(options) {}

void rt_kafka_network_t::shutdown() {
	for(auto& link : active_links_) {
		link.second->shutdown();
	}
}

void rt_kafka_network_t::send(const broker_addr_t& broker, kafka_rpc_t rpc) {
	std::unique_lock<mutex_t> guard(mutex_);

	auto& link = active_links_[broker];
	if(!link || link->is_closed()) {
		if(link && link->is_closed()) link->shutdown();

		link = std::make_shared<rt_kafka_link_t>(broker, scheduler_, options_);
	}

	link->send(rpc);
}

rt_kafka_cluster_t::rt_kafka_cluster_t(
			scheduler_ptr_t scheduler,
			kafka_network_ptr_t network,
			const broker_list_t& bootstrap_brokers,
			options_t options) :
		options_(options),
		bootstrap_brokers_(bootstrap_brokers),
		next_broker_(0),
		network_(network),
		metadata_correct_(false),
		rpc_queue_(4096) {
	if(bootstrap_brokers_.empty())
		throw std::runtime_error("bootstrap_brokers empty");

	routing_fiber_ = scheduler->start(&rt_kafka_cluster_t::router_loop, this);
}

future_t<void> rt_kafka_cluster_t::send(topic_request_ptr_t request, topic_response_ptr_t response) {
	topic_kafka_rpc_t rpc;
	rpc.request = request;
	rpc.response = response;

	if(!rpc_queue_.put(rpc)) {
		rpc.promise.set_exception(std::runtime_error("rt_kafka_cluster_t is shutting down"));
	}

	return rpc.promise.get_future();
}

void rt_kafka_cluster_t::route_rpc(topic_kafka_rpc_t rpc) {
	auto broker = metadata_.get_partition_leader_addr(rpc.request->topic, rpc.request->partition);

	network_->send(broker, rpc.to_kafka_rpc());

	rpc.promise.get_future().subscribe([this, rpc] (future_t<void> future) {
		if(future.has_exception() || rpc.response->err != kafka_err_t::NO_ERROR) {
			metadata_correct_ = false;
		}
	});
}

void rt_kafka_cluster_t::refresh_metadata() {
	kafka_rpc_t rpc;
	rpc.request = std::make_shared<metadata_request_t>();
	auto response = std::make_shared<metadata_response_t>();
	rpc.response = response;

	next_allowed_refresh_ = std::chrono::system_clock::now() + options_.lib.metadata_refresh_backoff;

	network_->send(get_next_broker(), rpc);

	rpc.promise.get_future().get();

	metadata_ = metadata_t(*response);
	metadata_correct_ = true;
}

void rt_kafka_cluster_t::router_loop() {
	topic_kafka_rpc_t rpc;

	while(!rpc_queue_.is_closed() && rpc_queue_.get(&rpc)) {
		try {
			if(!metadata_correct_ && std::chrono::system_clock::now() > next_allowed_refresh_) {
				refresh_metadata();
			}

			route_rpc(rpc);
		} catch(std::exception& e) {
			metadata_correct_ = false;
			rpc.promise.set_exception(std::current_exception());
		}
	}

	while(rpc_queue_.get(&rpc)) {
		rpc.promise.set_exception(std::runtime_error("rt_kafka_cluster_t is shutting down"));
	}
}

void rt_kafka_cluster_t::shutdown() {
	network_->shutdown();

	rpc_queue_.close();
	routing_fiber_.join();
}

const broker_addr_t& rt_kafka_cluster_t::get_next_broker() {
	return bootstrap_brokers_[(next_broker_++) % bootstrap_brokers_.size()];
}

}} // namespace raptor::kafka
