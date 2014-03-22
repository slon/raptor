#pragma once

#include <phantom/io_kafka/message_set.h>

namespace phantom { namespace io_kafka {

class producer_t {
public:
    virtual void produce(message_set_t message_set) = 0;

    virtual void flush() = 0;

    virtual void close() = 0;

    virtual ~producer_t() {}
};

}} // namespace phantom::io_kafka
