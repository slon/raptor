#include <atomic>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <raptor/core/scheduler.h>
#include <raptor/core/syscall.h>
#include <raptor/daemon/daemon.h>
#include <raptor/kafka/rt_kafka_client.h>

using namespace raptor;
using namespace raptor::kafka;

DEFINE_string(topic, "test", "topic name");
DEFINE_int32(n_partitions, 64, "number of partitions");
DEFINE_string(broker_list, "localhost:9092", "comma separated list of brokers");
DEFINE_int32(n_clients, 1, "number of clients");
DEFINE_int32(n_produce, 1000, "number of produce requests per client per partition");
DEFINE_int32(msg_size, 300, "size of single message");
DEFINE_int32(msg_set_size, 64 * 1024, "size of message set");
DEFINE_int32(sleep_ms, 50, "sleep interval between produce rpc in single partition");

std::atomic<size_t> TOTAL_BYTES_PRODUCED(0);

void run_client(scheduler_t* scheduler) {
	options_t options;

	options.lib.metadata_refresh_backoff = std::chrono::milliseconds(1);

	rt_kafka_client_t client(scheduler, options);
	for(auto broker : parse_broker_list(FLAGS_broker_list)) {
		LOG(INFO) << broker.first << " " << broker.second;
		client.add_broker(broker.first, broker.second);
	}

	std::string message(FLAGS_msg_size, 'f');

	std::vector<fiber_t> fibers;
	for(int i = 0; i < FLAGS_n_partitions; ++i) {
		fibers.push_back(scheduler->start([i, &client, message] () {
			for(int j = 0; j < FLAGS_n_produce; ++j) {
				try {
					message_set_builder_t builder(FLAGS_msg_set_size);
					while(builder.append(message.data(), message.size())) {}
					client.produce(FLAGS_topic, i, builder.build()).get();
					TOTAL_BYTES_PRODUCED += FLAGS_msg_set_size;
				} catch(std::exception& e) {
					LOG_EVERY_N(ERROR, 100) << e.what();
				}

				duration_t timeout = std::chrono::milliseconds(FLAGS_sleep_ms);
				rt_sleep(&timeout);
			}
		}));		
	}

	for(auto& f : fibers) f.join();
}


int main(int argc, char* argv[]) {
	google::ParseCommandLineFlags(&argc, &argv, true);
	setup_raptor();

	scheduler_t scheduler;

	std::vector<fiber_t> clients;

	for(int i = 0; i < FLAGS_n_clients; ++i) {
		clients.push_back(scheduler.start(run_client, &scheduler));
	}

	for(size_t i = 0; i < 1000; ++i) {
		int start_bytes = TOTAL_BYTES_PRODUCED;
		auto start_time = std::chrono::system_clock::now();

		sleep(2);

		int end_bytes = TOTAL_BYTES_PRODUCED;
		auto end_time = std::chrono::system_clock::now();

		std::cerr << float(end_bytes - start_bytes) / std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count() / 1024 / 1024 << std::endl;
	}

	for(auto& f : clients) f.join();

	return 0;
}
