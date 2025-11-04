#include <kea/hooks/hooks.h>
#include <kea/dhcp/dhcp4.h>
#include <kea/dhcp/pkt4.h>
#include <libpq-fe.h>
#include <sstream>
#include <iomanip>
#include <string>
#include <mutex>

using namespace isc::hooks;
using namespace isc::dhcp;
using namespace isc::data;

// Global variables for database configuration
std::string db_host = "localhost";
std::string db_name = "kea_db";
std::string db_user = "kea_user";
std::string db_password = "kea_password";
std::string conninfo;

extern "C" {

// Function to declare multi-threading compatibility
bool multi_threading_compatible() {
    return true;
}

int load(LibraryHandle& handle) {
    // Get the parameters from the Kea configuration
    ConstElementPtr params = handle.getParameters();

    if (params) {
        ConstElementPtr host = params->get("db_host");
        if (host) {
            db_host = host->stringValue();
        }
        ConstElementPtr name = params->get("db_name");
        if (name) {
            db_name = name->stringValue();
        }
        ConstElementPtr user = params->get("db_user");
        if (user) {
            db_user = user->stringValue();
        }
        ConstElementPtr password = params->get("db_password");
        if (password) {
            db_password = password->stringValue();
        }
    }

    // Build the connection string
    conninfo = "dbname=" + db_name +
               " user=" + db_user +
               " password=" + db_password +
               " host=" + db_host;

    return 0;
}

// Thread-safe function to get a database connection
PGconn* get_db_connection() {
    static thread_local PGconn* conn = nullptr;
    if (!conn) {
        conn = PQconnectdb(conninfo.c_str());
        if (PQstatus(conn) != CONNECTION_OK) {
            PQfinish(conn);
            conn = nullptr;
        }
    }
    return conn;
}

int unload() {
    return 0;
}

int pkt4_receive(CalloutHandle& handle) {
	try {
        Pkt4Ptr pkt4;
        handle.getArgument("query4", pkt4);
    
        OptionPtr vendor_class = pkt4->getOption(DHO_VENDOR_CLASS_IDENTIFIER);
    	if (!vendor_class) return 0;
    
        // Extract only the vendor class string
        const std::vector<uint8_t>& vendor_data = vendor_class->getData();
        std::string vendor_class_str(vendor_data.begin(), vendor_data.end());

        // Skip if vendor class does not start with "Cisco"
        if (vendor_class_str.find("Cisco") != 0) {
            std::cerr << "Vendor class " + vendor_class_str + " ignored" << std::endl;
            return 0;
        }

        HWAddrPtr hwaddr = pkt4->getHWAddr();
    
        if (!hwaddr) return 0;
    
        std::ostringstream mac_stream;
        for (size_t i = 0; i < hwaddr->hwaddr_.size(); ++i) {
            mac_stream << std::hex << std::setw(2) << std::setfill('0') << (int)hwaddr->hwaddr_[i];
            if (i < hwaddr->hwaddr_.size() - 1) mac_stream << ":";
        }
        std::string mac_address = mac_stream.str();
    
        PGconn* conn = get_db_connection();
        if (!conn) {
            std::cerr << "Failed to get database connection." << std::endl;
            return 0;
    	}

        // Prepare the statement
		const char* paramValues[2] = {mac_address.c_str(), vendor_class_str.c_str()};
		const char* prepare_stmt = "INSERT INTO cisco (mac, title, mtime) VALUES ($1, $2, extract (epoch from now()) on conflict (mac) do update set title = $1, mtime = extract (epoch from now())";

        PGresult* res = PQprepare(conn, "insert_vendor_class", prepare_stmt, 2, nullptr);

        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
            std::cerr << "Failed to prepare statement: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            return 0;
        }
        PQclear(res);

		res = PQexecPrepared(conn, "insert_vendor_class", 2, paramValues, nullptr, nullptr, 0);
        if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			std::cerr << "Failed to execute prepared statement: " << PQerrorMessage(conn) << std::endl;
            PQclear(res);
            return 0;
        }
        PQclear(res);

	} catch (const std::exception& e) {
		std::cerr << "Exception in pkt4_receive: " << e.what() << std::endl;
		return 0;
	}
    return 0;
}

extern "C" int version() {
    return KEA_HOOKS_VERSION;
}

} // extern "C"

