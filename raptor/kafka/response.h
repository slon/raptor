#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

#include <raptor/kafka/defs.h>
#include <raptor/kafka/wire.h>
#include <raptor/kafka/message_set.h>

namespace raptor { namespace kafka {

class response_t {
public:
    response_t() : correlation_id(0) {}

    void read(wire_reader_t* reader);

    int32_t correlation_id;

    virtual void read_body(wire_reader_t* reader) = 0;
};

typedef std::shared_ptr<response_t> response_ptr_t;

class partition_metadata_t {
public:
    kafka_err_t partition_err;
    int32_t partition_id;
    int32_t leader;
    std::vector<int32_t> replicas;
    std::vector<int32_t> in_sync_replicas;

    void read(wire_reader_t* reader);
};

class topic_metadata_t {
public:
    kafka_err_t topic_err;
    std::string name;
    std::vector<partition_metadata_t> partitions;

    void read(wire_reader_t* reader);
};

class metadata_response_t : public response_t {
public:
    metadata_response_t() {}

    class broker_t {
    public:
        int32_t node_id;
        std::string host;
        // for some reason port have type int32 in protocol
        int32_t port;

        void read(wire_reader_t* reader);
    };


    metadata_response_t(const std::vector<broker_t>& brokers,
                        const std::vector<topic_metadata_t>& topics)
        : brokers_(brokers), topics_(topics) {}

    const std::vector<broker_t>& brokers() const {
        return brokers_;
    }

    const std::vector<topic_metadata_t>& topics() const {
        return topics_;
    }

private:
    std::vector<broker_t> brokers_;
    std::vector<topic_metadata_t> topics_;

    virtual void read_body(wire_reader_t* reader);
};

typedef std::shared_ptr<metadata_response_t> metadata_response_ptr_t;

class fetch_response_t : public response_t {
public:
    fetch_response_t() = default;
    fetch_response_t(const std::string& t, partition_id_t p, kafka_err_t e, offset_t h, message_set_t m)
        : topic(t), partition(p), err(e), highwatermark_offset(h), message_set(m) {}

    std::string topic;
    partition_id_t partition;

    kafka_err_t err;
    offset_t highwatermark_offset;
    message_set_t message_set;

    virtual void read_body(wire_reader_t* reader);
};

typedef std::shared_ptr<fetch_response_t> fetch_response_ptr_t;

class produce_response_t : public response_t {
public:
    produce_response_t() = default;
    produce_response_t(const std::string& t, partition_id_t p, kafka_err_t e, offset_t o)
        : topic(t), partition(p), err(e), offset(o) {}

    std::string topic;
    partition_id_t partition;

    kafka_err_t err;
    offset_t offset;

    virtual void read_body(wire_reader_t* reader);
};

typedef std::shared_ptr<produce_response_t> produce_response_ptr_t;

class offset_response_t : public response_t {
public:
    kafka_err_t err;
    std::vector<offset_t> offsets;

    virtual void read_body(wire_reader_t* reader);
};

typedef std::shared_ptr<offset_response_t> offset_response_ptr_t;

}} // namespace raptor::kafka
