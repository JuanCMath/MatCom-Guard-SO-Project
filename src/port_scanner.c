#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "port_scanner.h"

void scan_ports() {
    printf("Escaneando puertos %d-%d...\n", PORT_RANGE_START, PORT_RANGE_END);
    for (int port = PORT_RANGE_START; port <= PORT_RANGE_END; port++) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr.s_addr = inet_addr("127.0.0.1")
        };

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            printf("[ABIERTO] Puerto: %d, Servicio: %s\n", port, get_service_name(port));
            close(sock);
        }
    }
}