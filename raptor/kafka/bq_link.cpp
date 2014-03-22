#include <phantom/io_kafka/bq_link.h>

#include <string.h>

#include <pd/base/log.H>
#include <pd/bq/bq_util.H>
#include <pd/bq/bq_job.H>

#include <phantom/pd.H>
#include <phantom/scheduler.H>

#include <phantom/io_kafka/options.h>
#include <phantom/io_kafka/request.h>
#include <phantom/io_kafka/response.h>

namespace phantom { namespace io_kafka {

void recv_all(const fd_t& fd, char* buff, size_t size) {
	size_t bytes_read = 0;
	while(bytes_read < size) {
		ssize_t n = pd::bq_read(fd.fd(), buff + bytes_read, size - bytes_read, NULL);

		if(n == 0) {
			throw exception_t("bq_read(): connection closed");
		}

		if(n < 0) {
			throw sys_exception_t("recv_all(): ", strerror(errno));
		}
		bytes_read += n;
	}
}

blob_t read_blob(const fd_t& fd) {
	int32_t blob_size;

	recv_all(fd, reinterpret_cast<char*>(&blob_size), sizeof(int32_t));
	blob_size = be32toh(blob_size);

	if(blob_size > 64 * 1024 * 1024) {
		throw exception_t("blob size > 64MB");
	}

	std::shared_ptr<char> blob(new char[blob_size], [] (char* p) { delete[] p; });
	recv_all(fd, blob.get(), blob_size);

	return blob_t(blob, blob_size);
}

bq_link_t::bq_link_t(fd_t socket, const options_t& options)
	: send_fd(socket.dup()), recv_fd(std::move(socket)), options(options) {}

bq_link_t::~bq_link_t() {
	close();
	active_coroutines.wait();
}

void bq_link_t::start(scheduler_t* scheduler) {
	active_coroutines.inc(2);

	bq_thr_t* thr = scheduler->bq_thr();

	bq_job(&bq_link_t::send_loop)(*this)->run(thr);
	bq_job(&bq_link_t::recv_loop)(*this)->run(thr);
}

future_t<void> bq_link_t::send(request_ptr_t request, response_ptr_t response) {
	task_t task;
	task.request = request;
	task.response = response;

	future_t<void> completed = task.promise.get_future();

	if(!send_channel.put(task)) { // connection closed
		throw std::runtime_error("bq_link_t is closed #0");
	}

	return completed;
}

void bq_link_t::send_loop() {
	char obuf[options.lib.obuf_size];
	bq_wire_writer_t writer(send_fd.fd(), obuf, sizeof(obuf));

	task_t task;

	while(send_channel.get(&task)) {
		try {
			task.request->write(&writer);
			writer.flush_all();

			if(!task.response)
				task.promise.set_value();

			if(!recv_channel.put(task)) {
				task.promise.set_exception(std::make_exception_ptr(std::runtime_error("bq_link_t is closed #1")));
			}
		} catch (...) {
			task.promise.set_exception(std::current_exception());
			break;
		}
	}

	close();

	std::exception_ptr err = std::make_exception_ptr(std::runtime_error("bq_link_t is closed #2"));
	while(send_channel.get(&task)) {
		task.promise.set_exception(err);
	}

	active_coroutines.dec();
}

void bq_link_t::recv_loop() {
	task_t task;

	while(recv_channel.get(&task)) {
		try {
			blob_t blob = read_blob(recv_fd);
			wire_reader_t reader(blob);
			task.response->read(&reader);
			task.promise.set_value();
		} catch(...) {
			task.promise.set_exception(std::current_exception());
			break;
		}
	}

	close();

	std::exception_ptr err = std::make_exception_ptr(std::runtime_error("bq_link_t is closed #3"));
	while(recv_channel.get(&task)) {
		task.promise.set_exception(err);
	}

	active_coroutines.dec();
}

void bq_link_t::close() {
	send_channel.close();
	recv_channel.close();
}

bool bq_link_t::is_closed() {
	return send_channel.is_closed();
}

}} // namespace phantom::io_kafka
