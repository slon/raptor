#include <raptor/kafka/options.h>

namespace raptor { namespace kafka {

options_t default_options() {
    options_t options;

    options.kafka.required_acks = 1;
    options.kafka.max_wait_time = 512;
    options.kafka.min_bytes = 64 * 1024;
    options.kafka.produce_timeout = -1;
    options.kafka.max_bytes = 4 * 1024 * 1024;

    options.lib.obuf_size = 1024;

    options.lib.metadata_refresh_backoff = std::chrono::seconds(1);

    return options;
}

}} // namespace raptor::kafka
