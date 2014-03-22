#include <raptor/kafka/response.h>

#include <iostream>

namespace raptor { namespace io_kafka {

std::ostream& operator << (std::ostream& stream,
                           const metadata_response_t& metadata);

std::ostream& operator << (std::ostream& stream,
                           const fetch_response_t& fetch_response);

std::ostream& operator << (std::ostream& stream,
                           const produce_response_t& produce_response);

}} // namespace raptor::io_kafka
