#include <gtest/gtest.h>

class integration_test_t : ::testing::Test {
public:
	void run_command(const std::string& command) {}

	std::string start_process(const std::string& command) {
		return "";
	}

	void stop_process(const std::string& process_id) {}
};
