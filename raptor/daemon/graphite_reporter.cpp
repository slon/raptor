#include <raptor/daemon/graphite_reporter.h>

#include <glog/logging.h>

#include <pm/metrics.h>
#include <pm/graphite.h>

#include <raptor/io/socket.h>
#include <raptor/io/inet_address.h>

namespace raptor {

graphite_reporter_t::graphite_reporter_t(scheduler_t* scheduler) :
	periodic_(scheduler, std::chrono::minutes(1), std::bind(&graphite_reporter_t::send_metrics, this)) {}

void graphite_reporter_t::send_metrics() {
	pm::graphite_printer_t printer(get_graphite_prefix());
	pm::get_root().print(&printer);

	duration_t timeout(1.);
	fd_guard_t graphite = socket_connect(inet_address_t::parse_ip_port("127.0.0.1", "42000"), &timeout);

	std::string graphite_report = printer.result();

	write_all(graphite.fd(), graphite_report.data(), graphite_report.size(), &timeout);
}

std::string get_graphite_prefix() {
	std::string fqdn = get_fqdn();

	for(size_t pos = 0; pos < fqdn.size(); ++pos) {
		if(fqdn[pos] == '.') fqdn[pos] = '_';
	}

	return fqdn;
}


} // namespace raptor
