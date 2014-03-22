#pragma once

#include <string>
#include <deque>

#include <pd/bq/bq_cond.H>

#include <phantom/io_kafka/consumer.h>
#include <phantom/io_kafka/options.h>
#include <phantom/io_kafka/hooks.h>

namespace phantom { namespace io_kafka {

class link_t;
class network_t;

class bq_consumer_t : public consumer_t {
public:
    bq_consumer_t(const options_t& options,
                  network_t* network,
                  io_kafka::hooks_t* hooks,
                  const std::string& topic,
                  partition_id_t partition,
                  offset_t offset);
    ~bq_consumer_t();

    void run();

    virtual message_set_t fetch();
    virtual void close();

private:
    std::string topic_;
    partition_id_t partition_;
    offset_t next_fetch_offset_;

    const options_t& options_;

    std::deque<message_set_t> queue_;
    pd::bq_cond_t cond_;

    bool offset_out_of_range_;
    bool is_running_, should_stop_;

    std::shared_ptr<link_t> link_;

    network_t* network_;
    hooks_t* hooks_;

    bool fetch_next(message_set_t* message_set);
};

}} // namespace phantom::io_kafka
