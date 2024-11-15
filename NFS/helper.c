#include "helper.h"

void get_local_ip(char *ip_address) {
    struct ifaddrs *ifaddr, *ifa;
    int family;
    char host[NI_MAXHOST];

    // Retrieve the linked list of network interfaces
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    // Iterate through each network interface
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || (ifa->ifa_flags & IFF_LOOPBACK)) {
            continue;  // Skip null addresses and loopback interfaces
        }

        family = ifa->ifa_addr->sa_family;

        // Check if the address is an IPv4 address
        if (family == AF_INET) {
            int s = getnameinfo(ifa->ifa_addr, sizeof(struct sockaddr_in),
                                host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            if (s != 0) {
                printf("getnameinfo() failed: %s\n", gai_strerror(s));
                exit(EXIT_FAILURE);
            }

            // Assign the first non-loopback IPv4 address found
            strncpy(ip_address, host, 15);
            ip_address[15] = '\0';  // Ensure null-termination
            break;
        }
    }

    freeifaddrs(ifaddr);
}