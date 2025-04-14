#ifndef PORT_SCANNER_H  
#define PORT_SCANNER_H  

#define PORT_RANGE_START 1
#define PORT_RANGE_END   1024

typedef struct {
    int port;
    char service[50];
    bool is_open;
} PortInfo;

void scan_ports();
const char* get_service_name(int port);

#endif  