# Kea DHCP Vendor Class Hook for PostgreSQL

This project implements a Kea DHCP hook in C++ to save client vendor class information to a PostgreSQL database. The hook is designed to be configurable via the Kea configuration file.

---

## Features

- Captures DHCP client vendor class information.
- Stores data in a PostgreSQL database.
- Configurable database connection parameters via Kea config file.
- Position Independent Code (PIC) compatible.

## Building

```bash
yum install isc-kea-devel boost-devel
g++ -fPIC -pthread -shared -o libvendor_class_hook.so vendor_class_hook.cpp -lkea-hooks -lpq -lboost_system -I/usr/include/kea
mv libvendor_class_hook.so /usr/lib64/kea/hooks/
```

## Configuration

Add the following to kea-dhcp4.conf. You can omit values same to default.

```json
{
  "Dhcp4": {
    "hooks-libraries": [
      {
        "library": "/usr/lib64/kea/hooks/libvendor_class_hook.so",
        "parameters": {
          "db_host": "localhost",
          "db_port": "5432",
          "db_name": "kea_db",
          "db_user": "kea_user",
          "db_password": "kea_password"
        }
      }
    ]
  }
}
```

Create table in your PostgreSQL database

```SQL
CREATE TABLE cisco (
    mac macaddr,
    title character varying,
    mtime integer,
    ip inet,
    uid character varying
);
```

---

## Credits
This code was developed with assistance from **Le Chat**, an AI assistant by Mistral AI, which provided guidance on Kea hook development, C++ implementation, and PostgreSQL integration.
