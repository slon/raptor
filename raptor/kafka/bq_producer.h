#pragma once

#include <deque>
#include <memory>

#include <pd/bq/bq_cond.H>

#include <raptor/kafka/defs.h>
#include <raptor/kafka/producer.h>
#include <raptor/kafka/options.h>
#include <raptor/kafka/hooks.h>

namespace phantom { namespace io_kafka {

class network_t;
class link_t;

class bq_producer_t : public producer_t {
public:
    bq_producer_t(const options_t& options,
                  network_t* network,
                  hooks_t* hooks,
                  const std::string& topic,
                  partition_id_t partition);
    ~bq_producer_t();

    void run();

    virtual void produce(message_set_t message_set);

    size_t queue_size();

    void stop();
    virtual void flush();
    virtual void close();

private:
    std::string topic_;
    partition_id_t partition_;

    const options_t& options_;

    std::deque<message_set_t> queue_;
    pd::bq_cond_t cond_;

    bool is_running_, should_stop_;

    pd::timeval_t last_refresh_;

    std::shared_ptr<link_t> link_;

    network_t* network_;
    hooks_t* hooks_;

    bool send(message_set_t message_set);
};

}} // namespace phantom::io_kafka
