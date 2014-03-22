#include <phantom/io_kafka/exception.h>

namespace phantom { namespace io_kafka {

void throw_kafka_err(const std::string& msg,
                     kafka_err_t err,
                     const std::string& topic,
                     partition_id_t partition) {
    if(err == kafka_err_t::OFFSET_OUT_OF_RANGE) {
        throw offset_out_of_range_t(msg, topic, partition);
    } else if(err == kafka_err_t::UNKNOWN_TOPIC_OR_PARTITION) {
        throw unknown_topic_or_partition_t(msg, topic, partition);
    } else {
        throw server_exception_t(msg, err, topic, partition);
    }
}

}} // namespace phantom::io_kafka
