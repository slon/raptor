#include <raptor/daemon/graphite_reporter.h>

#include <glog/logging.h>

#include <pm/metrics.h>
#include <pm/graphite.h>

#include <raptor/io/util.h>
#include <raptor/io/inet_address.h>

namespace raptor {

void write_graphite_metrics(int fd, duration_t* timeout) {
	pm::graphite_printer_t printer(get_graphite_prefix());
	pm::get_root().print(&printer);

	std::string graphite_report = printer.result();

	write_all(fd, graphite_report.data(), graphite_report.size(), timeout);
}

graphite_reporter_t::graphite_reporter_t(scheduler_ptr_t scheduler) :
	periodic_(scheduler, std::chrono::minutes(1), std::bind(&graphite_reporter_t::send_metrics, this)) {}

void graphite_reporter_t::send_metrics() {
	duration_t timeout(1.);

	auto addr = inet_address_t::parse_ip_port("127.0.0.1", "42000");
	fd_guard_t graphite = addr.connect(&timeout);

	write_graphite_metrics(graphite.fd(), &timeout);
}

void replace(std::string* str, char from, char to) {
	for(size_t pos = 0; pos < str->size(); ++pos) {
		if((*str)[pos] == from) (*str)[pos] = to;
	}
}

std::string get_graphite_prefix() {
	std::string fqdn = get_fqdn();
	replace(&fqdn, '.', '_');
	return "one_min." + fqdn;
}

void graphite_handler_t::on_accept(int fd) {
	duration_t timeout(1.);
	write_graphite_metrics(fd, &timeout);
}

} // namespace raptor
