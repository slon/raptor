#pragma once

#include <cstdlib>
#include <cassert>
#include <iostream>

#include <gmock/gmock.h>

#include <raptor/core/future.h>
#include <raptor/core/scheduler.h>
#include <raptor/client/bus.h>
#include <raptor/io/io_buff.h>
#include <raptor/kafka/wire.h>
#include <raptor/kafka/kafka_cluster.h>
#include <raptor/kafka/kafka_client.h>

using namespace raptor;
using namespace raptor::kafka;
using namespace ::testing;

inline char to_16(char c) {
	return (c < 10) ? (c + '0') : (c - 10 + 'A');
}

inline char from_16(char c) {
	return (c < 'A') ? (c - '0') : (c - 'A' + 10);
}

inline std::unique_ptr<io_buff_t> make_buff(const std::string& str) {
	auto buff = io_buff_t::create(str.length());
	memcpy((char*)buff->writable_data(), str.data(), str.length());
	buff->append(str.length());
	return std::move(buff);
}

inline std::string hexify(const std::string& str) {
	std::string hex;

	for(char c : str) {
		hex.push_back(to_16((c & 0xF0) >> 4));
		hex.push_back(to_16(c & 0x0F));
	}

	return hex;
}

inline std::string unhexify(const std::string& str) {
	assert(str.size() % 2 == 0);

	std::string result;
	for(size_t i = 0; i * 2 < str.size(); ++i) {
		result.push_back(
			(from_16(str[2 * i]) << 4) |
			from_16(str[2 * i + 1])
		);
	}

	return result;
}

inline std::string remove_spaces(const std::string& str) {
	std::string s;
	for(char c : str) {
		if(c != ' ') {
			s.push_back(c);
		}
	}
	return s;
}

struct mock_writer_t : public wire_writer_t {
	mock_writer_t(char* buffer, size_t size)
		: wire_writer_t(buffer, size) {};

	std::string data;

	virtual void flush() {
		if(!full_) {
			data += std::string(buffer_, wpos_);
		} else {
			data += std::string(buffer_, size_);
		}

		wpos_ = 0;
		full_ = false;
	}
};

struct mock_kafka_network_t : public kafka_network_t {
	MOCK_METHOD2(send, void(const broker_addr_t&, kafka_rpc_t));
	MOCK_METHOD0(shutdown, void());
};

class mock_kafka_client_t : public kafka_client_t {
	MOCK_METHOD2(get_log_end_offset, future_t<offset_t>(const std::string&, partition_id_t));
	MOCK_METHOD2(get_log_start_offset, future_t<offset_t>(const std::string&, partition_id_t));
	MOCK_METHOD3(fetch, future_t<message_set_t>(const std::string&, partition_id_t, offset_t));
	MOCK_METHOD3(produce, future_t<void>(const std::string&, partition_id_t, message_set_t));
};

class kafka_test_t : public ::testing::Test {
public:
	options_t options;
	scheduler_ptr_t scheduler;

	void SetUp() {
		scheduler = make_scheduler();
	}

	void TearDown() {
		scheduler->shutdown();
	}
};
