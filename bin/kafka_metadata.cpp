#include <iostream>

#include <gflags/gflags.h>

#include <raptor/kafka/rt_network.h>
#include <raptor/kafka/rt_link.h>
#include <raptor/io/inet_address.h>
#include <raptor/daemon/daemon.h>

DEFINE_string(host, "localhost", "kafka host");
DEFINE_int32(port, 9092, "kafka port");

using namespace raptor;
using namespace raptor::kafka;

int main(int argc, char* argv[]) {
	google::ParseCommandLineFlags(&argc, &argv, true);
	setup_raptor();

	scheduler_t scheduler;

	options_t opts;

	auto addr = inet_address_t::resolve_ip(FLAGS_host);
	addr.set_port(FLAGS_port);
	rt_link_t link(addr.connect(), opts);

	auto request = std::make_shared<metadata_request_t>();
	auto response = std::make_shared<metadata_response_t>();

	link.start(&scheduler);
	link.send(request, response).get();

	std::cout << *response;

	return 0;
}
