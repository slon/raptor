#pragma once

#include <raptor/kafka/defs.h>
#include <raptor/kafka/message_set.h>

namespace raptor { namespace kafka {

class producer_t {
public:
	void produce(partition_id_t partition, const std::string& message);
	void produce(partition_id_t partition, const message_t& message);
	
	void flush();
private:	

};

}} // namespace raptor::kafka
