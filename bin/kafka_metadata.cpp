#include <iostream>

#include <gflags/gflags.h>

#include <raptor/kafka/kafka_cluster.h>
#include <raptor/daemon/daemon.h>

DEFINE_string(host, "localhost", "kafka host");
DEFINE_int32(port, 9092, "kafka port");

using namespace raptor;
using namespace raptor::kafka;

int main(int argc, char* argv[]) {
	google::ParseCommandLineFlags(&argc, &argv, true);
	setup_raptor();

	auto scheduler = make_scheduler();

	options_t opts;

	duration_t timeout(0.4);
	rt_kafka_link_t link({ FLAGS_host, FLAGS_port }, scheduler, opts);

	auto request = std::make_shared<metadata_request_t>();
	auto response = std::make_shared<metadata_response_t>();

	kafka_rpc_t rpc;
	rpc.request = request;
	rpc.response = response;

	link.send(rpc);
	rpc.promise.get_future().get();

	std::cout << *response;

	link.shutdown();
	scheduler->shutdown();

	return 0;
}
