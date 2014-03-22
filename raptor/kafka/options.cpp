#include <raptor/kafka/options.h>

namespace raptor { namespace io_kafka {

options_t default_options() {
    options_t options;

    options.kafka.required_acks = 1;
    options.kafka.max_wait_time = 512;
    options.kafka.min_bytes = 64 * 1024;
    options.kafka.produce_timeout = -1;
    options.kafka.max_bytes = 4 * 1024 * 1024;

    options.lib.consumer_timeout = 512 * pd::interval::millisecond;
    options.lib.obuf_size = 1024;

    options.lib.metadata_refresh_backoff = pd::interval::second;

    return options;
}

}} // namespace raptor::io_kafka
