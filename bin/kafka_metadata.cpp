#include <iostream>

#include <boost/program_options.hpp>

#include <raptor/kafka/rt_network.h>
#include <raptor/kafka/rt_link.h>

namespace po = boost::program_options;

bool parse_options(int argc, char* argv[], po::variables_map* map) {
	po::options_description desc("fetch metadata from kafka broker");
	desc.add_options()
		("help", "show this help message")
		("host", po::value<std::string>(), "broker host")
		("port", po::value<int>()->default_value(9092), "broker port")
	;

	po::store(po::parse_command_line(argc, argv, desc), *map);
	po::notify(*map);

	if(map->count("help")) {
		std::cerr << desc << std::endl;
		return false;
	}

	return true;
}

using namespace raptor;

int main(int argc, char* argv[]) {
	po::variables_map options;
	if(!parse_options(argc, argv, &options)) return -1;

	scheduler_t scheduler;

	kafka::options_t opts;

	kafka::rt_link_t link(
		kafka::connect(options["host"].as<std::string>(), options["port"].as<int>()),
		opts
	);

	auto request = std::make_shared<kafka::metadata_request_t>();
	auto response = std::make_shared<kafka::metadata_response_t>();

	link.start(&scheduler);
	link.send(request, response).get();

	std::cout << *response;

	link.close();
	scheduler.shutdown();

	return 0;
}
