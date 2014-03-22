#include <raptor/kafka/defs.h>

namespace raptor { namespace io_kafka {

const std::string DEFAULT_CLIENT_ID = "raptor/kafka";

char const* kafka_err_str(kafka_err_t err) {
    switch(err) {
    case kafka_err_t::NO_ERROR:
        return "NO_ERROR";
    case kafka_err_t::UNKNOWN:
        return "UNKNOWN";
    case kafka_err_t::OFFSET_OUT_OF_RANGE:
        return "OFFSET_OUT_OF_RANGE";
    case kafka_err_t::INVALID_MESSAGE:
        return "INVALID_MESSAGE";
    case kafka_err_t::UNKNOWN_TOPIC_OR_PARTITION:
        return "UNKNOWN_TOPIC_OR_PARTITION";
    case kafka_err_t::INVALID_MESSAGE_SIZE:
        return "INVALID_MESSAGE_SIZE";
    case kafka_err_t::LEADER_NOT_AVAILABLE:
        return "LEADER_NOT_AVAILABLE";
    case kafka_err_t::NOT_LEADER_FOR_PARTITION:
        return "NOT_LEADER_FOR_PARTITION";
    case kafka_err_t::REQUEST_TIMED_OUT:
        return "REQUEST_TIMED_OUT";
    case kafka_err_t::BROKER_NOT_AVAILABLE:
        return "BROKER_NOT_AVAILABLE";
    case kafka_err_t::REPLICA_NOT_AVAILABLE:
        return "REPLICA_NOT_AVAILABLE";
    case kafka_err_t::MESSAGE_SIZE_TOO_LARGE:
        return "MESSAGE_SIZE_TOO_LARGE";
    case kafka_err_t::STALE_CONTROLLER_EPOCH_CODE:
        return "STALE_CONTROLLER_EPOCH_CODE";
    case kafka_err_t::OFFSET_METADATA_TOO_LARGE_CODE:
        return "OFFSET_METADATA_TOO_LARGE_CODE";
    default:
        return "UNKNOWN UNKNOWN";
    }
}

}} // namespace raptor::io_kafka
