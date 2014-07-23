#include <gtest/gtest.h>

#include <thread>

#include <raptor/kafka/kafka_cluster.h>

#include "common.h"

class kafka_cluster_test_t : public kafka_test_t {
public:
	std::shared_ptr<mock_kafka_network_t> network;
	kafka_cluster_ptr_t cluster;

	void SetUp() {
		kafka_test_t::SetUp();

		network = std::make_shared<mock_kafka_network_t>();
		EXPECT_CALL(*network, shutdown());
	}

	void create_cluster() {
		broker_list_t brokers = { { "test", 9092 }, { "test", 9093 } };

		options.lib.metadata_refresh_backoff = std::chrono::milliseconds(5);

		cluster.reset(new rt_kafka_cluster_t(
			scheduler,
			network,
			brokers,
			options
		));
	}

	void TearDown() {
		if(cluster) cluster->shutdown();

		kafka_test_t::TearDown();
	}

	static metadata_response_t make_test_metadata_response() {
		std::vector<metadata_response_t::broker_t> brokers = {
			{ 0, "test", 9092 }, { 1, "test", 9093 }, { 2, "prod", 9090 }
		};
		std::vector<topic_metadata_t> topics;
		topics.resize(1);

		topics[0].topic_err = kafka_err_t::NO_ERROR;
		topics[0].name = "topic-1";
		topics[0].partitions = {
			{ kafka_err_t::NO_ERROR, 0, 0, {}, {} },
			{ kafka_err_t::NO_ERROR, 1, 1, {}, {} },
			{ kafka_err_t::NO_ERROR, 2, 2, {}, {} }
		};

		return metadata_response_t(brokers, topics);
	}

	topic_request_ptr_t make_request(const std::string& topic, partition_id_t partition) {
		return std::make_shared<offset_request_t>(topic, partition, 0, 0);
	}

	topic_response_ptr_t make_good_response() {
		return topic_response_ptr_t(new offset_response_t("", 0, kafka_err_t::NO_ERROR, {}));
	}

	topic_response_ptr_t make_bad_response() {
		return topic_response_ptr_t(new offset_response_t("", 0, kafka_err_t::LEADER_NOT_AVAILABLE, {}));
	}
};

ACTION(CompleteRPC) { arg1.promise.set_value(); }
ACTION(FailRPC) { arg1.promise.set_exception(std::runtime_error("fail")); }

MATCHER(IsMetadataRequest, "") {
	return std::dynamic_pointer_cast<metadata_request_t>(arg.request) != nullptr;
}

TEST_F(kafka_cluster_test_t, shutdown) {
	create_cluster();

	std::this_thread::sleep_for(options.lib.metadata_refresh_backoff / 2);
}

ACTION(ReturnMetadata) {
	auto metadata = std::dynamic_pointer_cast<metadata_response_t>(arg1.response);
	*metadata = kafka_cluster_test_t::make_test_metadata_response();
	arg1.promise.set_value();
}

ACTION(FillExceptionPromise) {
	arg1.promise.set_exception(std::runtime_error("send failed"));
}

TEST_F(kafka_cluster_test_t, routes_requests_according_to_metadata) {
	InSequence s;

	EXPECT_CALL(*network, send(_, IsMetadataRequest()))
		.WillOnce(ReturnMetadata());

	create_cluster();

	EXPECT_CALL(*network, send(broker_addr_t{ "test", 9093 }, _)).WillOnce(CompleteRPC());
	EXPECT_CALL(*network, send(broker_addr_t{ "prod", 9090 }, _)).WillOnce(CompleteRPC());
	EXPECT_CALL(*network, send(broker_addr_t{ "test", 9092 }, _)).WillOnce(CompleteRPC());

	cluster->send(make_request("topic-1", 1), make_good_response()).get();
	cluster->send(make_request("topic-1", 2), make_good_response()).get();
	cluster->send(make_request("topic-1", 0), make_good_response()).get();
}

TEST_F(kafka_cluster_test_t, refreshes_metadata_if_topic_not_found) {
	InSequence s;
	// return empty metadata first time
	EXPECT_CALL(*network, send(_, IsMetadataRequest()))
		.WillOnce(CompleteRPC());

	// expect metadata refresh because topic doesn't exists
	EXPECT_CALL(*network, send(_, IsMetadataRequest()))
		.WillOnce(ReturnMetadata());

	EXPECT_CALL(*network, send(_, Not(IsMetadataRequest())))
		.WillOnce(CompleteRPC());

	create_cluster();

	auto future = cluster->send(make_request("topic-1", 0), make_good_response());	
	future.wait(NULL);
	ASSERT_TRUE(future.has_exception());

	std::this_thread::sleep_for(options.lib.metadata_refresh_backoff * 2);

	future = cluster->send(make_request("topic-1", 0), make_good_response());
	future.wait(NULL);
	future.get();
}

TEST_F(kafka_cluster_test_t, refreshes_metadata_on_failure) {
	InSequence s;
	EXPECT_CALL(*network, send(_, IsMetadataRequest()))
		.WillOnce(ReturnMetadata());

	// expect request would be sent
	EXPECT_CALL(*network, send(_, Not(IsMetadataRequest())))
		.WillOnce(CompleteRPC());

	// expect metadata refresh because of bad request
	EXPECT_CALL(*network, send(_, IsMetadataRequest()))
		.WillOnce(ReturnMetadata());

	// expect second request would be sent and will return exception
	EXPECT_CALL(*network, send(_, Not(IsMetadataRequest())))
		.WillOnce(FillExceptionPromise());

	// expect metadata refresh because of exception during request
	EXPECT_CALL(*network, send(_, IsMetadataRequest()))
		.WillOnce(ReturnMetadata());

	// final correct request
	EXPECT_CALL(*network, send(_, Not(IsMetadataRequest())))
		.WillOnce(CompleteRPC());

	create_cluster();

	auto future = cluster->send(make_request("topic-1", 0), make_bad_response());
	future.wait(NULL);
	ASSERT_TRUE(future.has_value());

	std::this_thread::sleep_for(options.lib.metadata_refresh_backoff * 2);

	future = cluster->send(make_request("topic-1", 0), make_good_response());
	future.wait(NULL);
	ASSERT_TRUE(future.has_exception());

	std::this_thread::sleep_for(options.lib.metadata_refresh_backoff * 2);

	future = cluster->send(make_request("topic-1", 0), make_good_response());
	future.wait(NULL);
	ASSERT_TRUE(future.has_value());
}

TEST_F(kafka_cluster_test_t, stress_test) {
	EXPECT_CALL(*network, send(_, IsMetadataRequest()))
		.WillRepeatedly(ReturnMetadata());

	EXPECT_CALL(*network, send(_, Not(IsMetadataRequest())))
		.WillRepeatedly(CompleteRPC());

	create_cluster();

	for(int i = 0; i < 10000; ++i) {
		cluster->send(make_request("topic-1", 0), make_bad_response());
	}
}
