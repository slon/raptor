#pragma once

#include <raptor/kafka/message_set.h>

namespace phantom { namespace io_kafka {

class consumer_t {
public:
    virtual message_set_t fetch() = 0;

    virtual void close() = 0;

    virtual ~consumer_t() {}
};

}} // namespace phantom::io_kafka
