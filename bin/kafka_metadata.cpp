#include <iostream>

#include <gflags/gflags.h>

#include <raptor/kafka/rt_network.h>
#include <raptor/kafka/rt_link.h>
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

	rt_link_t link(connect_socket(FLAGS_host, FLAGS_port), opts);

	auto request = std::make_shared<metadata_request_t>();
	auto response = std::make_shared<metadata_response_t>();

	link.start(&scheduler);
	link.send(request, response).get();

	std::cout << *response;

	return 0;
}
