#include <raptor/kafka/options.h>

namespace raptor { namespace kafka {

options_t default_options() {
	return options_t();
}


void set_default_options(options_t* options) {
    options->kafka.required_acks = 1;
    options->kafka.max_wait_time = 50;
    options->kafka.min_bytes = 64 * 1024;
    options->kafka.produce_timeout = -1;
    options->kafka.max_bytes = 4 * 1024 * 1024;

    options->lib.obuf_size = 1024;

	options->lib.producer_buffer_size = 64 * 1024;
	options->lib.producer_max_outstanding_requests = 16;

    options->lib.metadata_refresh_backoff = std::chrono::milliseconds(150);
}

std::vector<std::string> split(const std::string& str, char by) {
	std::vector<std::string> splitted;

	size_t last_split = 0;
	for(size_t split_pos = 0; split_pos != str.size(); ++split_pos) {
		if(str[split_pos] == by) {
			splitted.push_back(str.substr(last_split, split_pos - last_split));
			last_split = split_pos + 1;
		}
	}

	splitted.push_back(str.substr(last_split, str.size() - last_split));

	return splitted;
}

std::vector<std::pair<std::string, uint16_t>> parse_broker_list(const std::string& list_string) {
	std::vector<std::pair<std::string, uint16_t>> list;

	for(const std::string& host : split(list_string, ',')) {
		auto addr_pair = split(host, ':');

		if(addr_pair.size() != 2 || addr_pair[0].empty() || addr_pair[1].empty())
			throw std::runtime_error("invalid address");

		list.emplace_back(addr_pair[0], atoi(addr_pair[1].c_str()));
	}

	return list;
}


}} // namespace raptor::kafka
