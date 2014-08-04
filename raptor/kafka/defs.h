#pragma once

#include <cstdint>
#include <string>

namespace raptor {

namespace kafka {

typedef int32_t partition_id_t;
typedef int64_t offset_t;

static const int API_VERSION = 0;

extern const std::string DEFAULT_CLIENT_ID;

typedef int32_t host_id_t;

enum class api_key_t : int16_t {
    PRODUCE_REQUEST = 0,
    FETCH_REQUEST = 1,
    OFFSET_REQUEST = 2,
    METADATA_REQUEST = 3,
// This two make no sense on the client
//    LEADER_AND_ISR_REQUEST = 4,
//    STOP_REPLICA_REQUEST = 5,
    OFFSET_COMMIT_REQUEST = 6,
    OFFSET_FETCH_REQUEST = 7
};

enum class kafka_err_t {
    NO_ERROR = 0,
    UNKNOWN = -1,
    OFFSET_OUT_OF_RANGE = 1,
    INVALID_MESSAGE = 2,
    UNKNOWN_TOPIC_OR_PARTITION = 3,
    INVALID_MESSAGE_SIZE = 4,
    LEADER_NOT_AVAILABLE = 5,
    NOT_LEADER_FOR_PARTITION = 6,
    REQUEST_TIMED_OUT = 7,
    BROKER_NOT_AVAILABLE = 8,
    REPLICA_NOT_AVAILABLE = 9,
    MESSAGE_SIZE_TOO_LARGE = 10,
    STALE_CONTROLLER_EPOCH_CODE = 11,
    OFFSET_METADATA_TOO_LARGE_CODE = 12
};

//! returns c-string description of kafka error
char const* kafka_err_str(kafka_err_t err);

enum class compression_codec_t : int8_t {
	NONE = 0,
	GZIP = 1,
	SNAPPY = 2
};

}} // namespace raptor::kafka
