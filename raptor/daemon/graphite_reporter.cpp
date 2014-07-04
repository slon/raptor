#include <raptor/daemon/graphite_reporter.h>

#include <glog/logging.h>

#include <pm/metrics.h>
#include <pm/graphite.h>

#include <raptor/io/util.h>
#include <raptor/io/inet_address.h>

namespace raptor {

graphite_reporter_t::graphite_reporter_t(scheduler_ptr_t scheduler) :
	periodic_(scheduler, std::chrono::minutes(1), std::bind(&graphite_reporter_t::send_metrics, this)) {}

void graphite_reporter_t::send_metrics() {
	pm::graphite_printer_t printer(get_graphite_prefix());
	pm::get_root().print(&printer);

	auto addr = inet_address_t::parse_ip_port("127.0.0.1", "42000");
	duration_t timeout(1.);
	fd_guard_t graphite = addr.connect(&timeout);

	std::string graphite_report = printer.result();

	write_all(graphite.fd(), graphite_report.data(), graphite_report.size(), &timeout);
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

} // namespace raptor
