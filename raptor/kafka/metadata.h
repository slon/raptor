#pragma once

#include <string>
#include <map>
#include <set>

#include <raptor/kafka/response.h>

namespace raptor { namespace io_kafka {

class metadata_t {
public:
    struct addr_t {
        std::string hostname;
        uint16_t port;
    };

    metadata_t() : next_bootstrap_broker_(0) {}

    void update(const metadata_response_t& response);

    addr_t get_host_addr(host_id_t broker_id) const;
    host_id_t get_partition_leader(const std::string& topic,
                                   partition_id_t partition) const;

    addr_t get_next_broker();
    void add_broker(const std::string& host, uint16_t port);

private:
    std::map<host_id_t, addr_t> brokers_;
    std::map<std::string, std::map<partition_id_t, host_id_t>> topics_;

    std::vector<addr_t> bootstrap_brokers_;
    size_t next_bootstrap_broker_;
};

}} // namespace raptor::io_kafka
