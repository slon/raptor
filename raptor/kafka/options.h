#pragma once

#include <vector>
#include <string>
#include <stdexcept>
#include <utility>

#include <raptor/core/time.h>

namespace raptor { namespace kafka {

struct options_t;

void set_default_options(options_t* options);

struct options_t {
	options_t() { set_default_options(this); }

    struct kafka_t {
        /**
         * This field indicates how many acknowledgements the servers
         * should receive before responding to the request.
         *
         * If it is 0 the server does not send any response. If it is 1,
         * the server will wait the data is written to the local log
         * before sending a response. If it is -1 the server will block
         * until the message is committed by all in sync replicas before
         * sending a response. For any number > 1 the server will block
         * waiting for this number of acknowledgements to occur (but the
         * server will never wait for more acknowledgements than there are
         * in-sync replicas).
         */
        int required_acks;

        /**
         * The max wait time(in milliseconds) is the maximum amount of
         * time broker will block waiting if insufficient data is
         * available at the time the request is issued.
         */
        int max_wait_time;

        /**
         * This is the minimum number of bytes of messages that must be
         * available to give a response. If the client sets this to 0 the
         * server will always respond immediately, however if there is no
         * new data since their last request they will just get back empty
         * message sets. If this is set to 1, the server will respond as
         * soon as at least one partition has at least 1 byte of data or
         * the specified timeout occurs. By setting higher values in
         * combination with the timeout the consumer can tune for
         * throughput and trade a little additional latency for reading
         * only large chunks of data (e.g. setting MaxWaitTime to 100 ms
         * and setting MinBytes to 64k would allow the server to wait up
         * to 100ms to try to accumulate 64k of data before responding).
         */
        int min_bytes;

        /**
         * This provides a maximum time the server can await the
         * receipt of the number of acknowledgements in RequiredAcks.
         * The timeout is not an exact limit on the request time for a
         * few reasons: (1) it does not include network latency, (2)
         * the timer begins at the beginning of the processing of this
         * request so if many requests are queued due to server
         * overload that wait time will not be included, (3) we will
         * not terminate a local write so if the local write time
         * exceeds this timeout it will not be respected. To get a
         * hard timeout of this type the client should use the socket
         * timeout.
         */
        int produce_timeout;

        /**
         *  The maximum bytes broker will include in message set. This
         *  helps bound the size of the response.
         */
        int max_bytes;
    } kafka;

    struct library_t {
        duration_t socket_timeout;
        int so_rcvbuf;
        int so_sndbuf;

        duration_t metadata_refresh_backoff;

        int obuf_size;

		duration_t link_timeout;

		size_t producer_buffer_size;
		size_t producer_max_outstanding_requests;
    } lib;
};

typedef std::pair<std::string, uint16_t> broker_addr_t;
typedef std::vector<broker_addr_t> broker_list_t;

broker_list_t parse_broker_list(const std::string& brokerlist);

}} // namespace raptor::kafka
