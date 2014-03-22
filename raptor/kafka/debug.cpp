#include <raptor/kafka/debug.h>

#include <string>

namespace phantom { namespace io_kafka {

std::string maybe_err_str(kafka_err_t err) {
    if(err != kafka_err_t::NO_ERROR) {
        return std::string("(") + kafka_err_str(err) + ")";
    } else {
        return "";
    }
}

std::ostream& operator << (std::ostream& stream,
                           const metadata_response_t& metadata) {
    stream << "metadata:" << std::endl;
    stream << "  brokers:" << std::endl;

    for(const auto& broker : metadata.brokers()) {
        stream << "    " << broker.node_id << " - "
               << broker.host << ":" << broker.port << std::endl;
    }

    stream << "  topics:" << std::endl;
    for(const auto& topic : metadata.topics()) {
        stream << "    " << topic.name
               << maybe_err_str(topic.topic_err) << ":" << std::endl;

        for(const auto& partition : topic.partitions) {
            stream << "      " << partition.partition_id
                   << maybe_err_str(partition.partition_err)
                   << ": leader - " << partition.leader
                   << " replicas -";
            for(int node : partition.replicas) {
                stream << " " << node;
            }

            stream << " in_sync_replicas -";
            for(int node : partition.in_sync_replicas) {
                stream << " " << node;
            }
            stream << std::endl;
        }
    }

    return stream;
}

std::ostream& operator << (std::ostream& stream,
                           const fetch_response_t& fetch_response) {
    stream << "fetch:" << std::endl;

    return stream;
}

std::ostream& operator << (std::ostream& stream,
                           const produce_response_t& produce_response) {
    stream << "produce:" << std::endl;

    return stream;
}


}} // namespace phantom::io_kafka
