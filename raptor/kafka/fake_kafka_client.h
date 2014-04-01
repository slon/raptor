#include <string>
#include <deque>
#include <utility>
#include <vector>
#include <map>

#include <raptor/core/mutex.h>

#include <raptor/kafka/kafka_client.h>

namespace raptor { namespace kafka {

class fake_log_t {
public:
	fake_log_t() : start_offset_(0), max_size_(0) {}
	fake_log_t(size_t max_size) : start_offset_(0), max_size_(max_size) {}

	void produce(message_set_t msg_set) {
		auto iter = msg_set.iter();

		while(!iter.is_end()) {
			auto msg = iter.next();
			log_.push_back(std::string(msg.value, msg.value_size));
		}

		while(log_.size() > max_size_) {
			++start_offset_;
			log_.pop_front();
		}
	}

	message_set_t fetch(offset_t offset) {
		if(offset < start_offset_ || size_t(offset - start_offset_) > log_.size())
			throw offset_out_of_range_t("");

		message_set_builder_t builder(4 * 1024);

		for(size_t i = 0; offset + i < start_offset_ + log_.size(); ++i) {
			const auto& msg = log_[(offset - start_offset_) + i];

			if(!builder.append(msg.data(), msg.size()))
				break;
		}

		return builder.build();
	}

	offset_t start_offset() {
		return start_offset_;
	}

	offset_t end_offset() {
		return start_offset_ + log_.size();
	}

private:
	offset_t start_offset_;
	size_t max_size_;
	std::deque<std::string> log_;
};

class fake_kafka_client_t : public kafka_client_t {
public:
	fake_kafka_client_t(int max_log_size) : max_log_size_(max_log_size) {}

	virtual future_t<offset_t> get_log_end_offset(const std::string& topic, partition_id_t partition) {
		std::unique_lock<mutex_t> guard(mutex_);
		return make_ready_future(get_log(topic, partition)->end_offset());
	}

	virtual future_t<offset_t> get_log_start_offset(const std::string& topic, partition_id_t partition) {
		std::unique_lock<mutex_t> guard(mutex_);
		return make_ready_future(get_log(topic, partition)->start_offset());
	}

	virtual future_t<message_set_t> fetch(const std::string& topic, partition_id_t partition, offset_t offset) {
		std::unique_lock<mutex_t> guard(mutex_);
		return make_ready_future(get_log(topic, partition)->fetch(offset));
	}

	virtual future_t<void> produce(const std::string& topic, partition_id_t partition, message_set_t msg_set) {
		std::unique_lock<mutex_t> guard(mutex_);
		get_log(topic, partition)->produce(msg_set);
		return make_ready_future();
	}

private:
	mutex_t mutex_;

	size_t max_log_size_;
	std::map<std::pair<std::string, partition_id_t>, fake_log_t> logs_;

	fake_log_t* get_log(const std::string topic, partition_id_t partition) {
		auto key = std::make_pair(topic, partition);

		if(logs_.find(key) == logs_.end()) {
			logs_[key] = fake_log_t(max_log_size_);
		}

		return &(logs_[key]);
	}
};

}} // namespace raptor::kafka
