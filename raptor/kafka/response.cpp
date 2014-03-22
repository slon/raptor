#include <raptor/kafka/response.h>

#include <raptor/kafka/exception.h>

namespace phantom { namespace io_kafka {

static const int32_t MAX_REPLICAS = 128;
static const int32_t MAX_PARTITIONS = 4096;
static const int32_t MAX_BROKERS = 4096;
static const int32_t MAX_TOPICS = 65536;
static const int32_t MAX_OFFSETS = 65536;

void response_t::read(wire_reader_t* reader) {
    correlation_id = reader->int32();

    read_body(reader);
}

void metadata_response_t::broker_t::read(wire_reader_t* reader) {
    node_id = reader->int32();
    reader->string(&host);
    port = reader->int32();
}

void partition_metadata_t::read(wire_reader_t* reader) {
    partition_err = static_cast<kafka_err_t>(reader->int16());
    partition_id = reader->int32();
    leader = reader->int32();

    replicas.resize(check_range(
        reader->array_size(), 0, MAX_REPLICAS,
        "partition.replicas"
    ));
    for(int32_t& node_id : replicas) {
        node_id = reader->int32();
    }

    in_sync_replicas.resize(check_range(
        reader->array_size(), 0, MAX_REPLICAS,
        "partition.in_sync_replicas"
    ));
    for(int32_t& node_id : in_sync_replicas) {
        node_id = reader->int32();
    }
}

void topic_metadata_t::read(wire_reader_t* reader) {
    topic_err = static_cast<kafka_err_t>(reader->int16());
    reader->string(&name);

    partitions.resize(check_range(
        reader->array_size(), 0, MAX_PARTITIONS,
        "topic.partitions"
    ));
    for(partition_metadata_t& partition : partitions) {
        partition.read(reader);
    }
}

void metadata_response_t::read_body(wire_reader_t* reader) {
    brokers_.resize(check_range(
        reader->array_size(), 0, MAX_BROKERS,
        "metadata.brokers"
    ));
    for(auto& broker : brokers_) {
        broker.read(reader);
    }

    topics_.resize(check_range(
        reader->array_size(), 0, MAX_TOPICS,
        "metadata.topics"
    ));
    for(auto& topic : topics_) {
        topic.read(reader);
    }
}

void fetch_response_t::read_body(wire_reader_t* reader) {
    check(1, reader->array_size(), "fetch.n_topics");

    if(!reader->string(&topic)) {
        throw exception_t("fetch.topic is null");
    }

    check(1, reader->array_size(), "fetch.n_partitions");

    partition = reader->int32();
    err = static_cast<kafka_err_t>(reader->int16());
    highwatermark_offset = reader->int64();

    message_set.read(reader);
}

void produce_response_t::read_body(wire_reader_t* reader) {
    check(1, reader->array_size(), "produce.n_topics");

    if(!reader->string(&topic)) {
        throw exception_t("produce.topic is null");
    }

    check(1, reader->array_size(), "produce.n_partitions");

    partition = reader->int32();
    err = static_cast<kafka_err_t>(reader->int16());
    offset = reader->int64();
}

void offset_response_t::read_body(wire_reader_t* reader) {
    check(1, reader->array_size(), "offset.n_topics");
    std::string topic_name;
    if(!reader->string(&topic_name)) {
        throw exception_t("offset.topic_name is null");
    }

    check(1, reader->array_size(), "offset.n_partitions");
    reader->int32(); // skip partition_id

    err = static_cast<kafka_err_t>(reader->int16());

    size_t n_offsets = check_range(reader->array_size(), 0, MAX_OFFSETS,
                                   "offset.offsets");

    offsets.resize(n_offsets);
    for(size_t i = 0; i < n_offsets; ++i) {
        offsets[i] = reader->int64();
    }
}

}} // namespace phantom::io_kafka
