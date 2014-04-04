#include <gflags/gflags.h>
#include <glog/logging.h>

// for scheduler_t
#include <raptor/core/scheduler.h>

// for rt_kafka_client_t
#include <raptor/kafka/rt_kafka_client.h>

// for options_t and parse_broker_list()
#include <raptor/kafka/options.h>

// for setup_rator()
#include <raptor/daemon/daemon.h>

DEFINE_string(topic, "test", "topic name");
DEFINE_int32(partition, 0, "partition");
DEFINE_int32(offset, 0, "offset");
DEFINE_string(broker_list, "localhost:9092", "comma separated list of brokers");

using namespace raptor;
using namespace raptor::kafka;

int main(int argc, char* argv[]) {
	google::ParseCommandLineFlags(&argc, &argv, true);

	setup_raptor();

	scheduler_t scheduler;

	options_t options;

	rt_kafka_client_t client(&scheduler, parse_broker_list(FLAGS_broker_list), options);

	try {
		// start async request
		future_t<message_set_t> request = client.fetch(FLAGS_topic, FLAGS_partition, FLAGS_offset);

		// wait for request to finish
		message_set_t msgset = request.get();

		// message set supports iteration
		auto iter = msgset.iter();
		while(!iter.is_end()) {
			message_t message = iter.next();

			std::cout << "offset: " << message.offset;
			std::cout << " value: ";

			if(message.value == NULL) {
				std::cout << "NULL";
			} else {
				// NOTE: message.value is NOT zero terminated
				std::string value(message.value, message.value_size);

				std::cout << "size(" << value.size() << ")";
			}

			std::cout << std::endl;
		}
	} catch(const std::exception& e) {
		LOG(ERROR) << e.what();
	}

	return 0;
}
