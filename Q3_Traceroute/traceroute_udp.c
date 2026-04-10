#ifndef _WIN32
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define PROBES_PER_TTL 3
#define MAX_PACKET_SIZE 128

static double time_diff_ms(const struct timespec *start, const struct timespec *end) {
    double sec = (double)(end->tv_sec - start->tv_sec) * 1000.0;
    double nsec = (double)(end->tv_nsec - start->tv_nsec) / 1000000.0;
    return sec + nsec;
}

static int resolve_ipv4(const char *host, struct sockaddr_in *addr) {
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    char ip_buffer[INET_ADDRSTRLEN];

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(host, NULL, &hints, &result) != 0) {
        return -1;
    }

    memcpy(addr, result->ai_addr, sizeof(struct sockaddr_in));
    inet_ntop(AF_INET, &addr->sin_addr, ip_buffer, sizeof(ip_buffer));
    printf("traceroute to %s (%s), %d hops max, %d probes per hop\n", host, ip_buffer, 30, PROBES_PER_TTL);
    freeaddrinfo(result);
    return 0;
}

int main(int argc, char *argv[]) {
    const char *target_host;
    int max_hops = 30;
    int dest_port = 33434;
    int udp_sock = -1;
    int icmp_sock = -1;
    struct sockaddr_in dest_addr;
    struct timeval timeout;

    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: sudo %s <destination> [max_hops] [dest_port]\n", argv[0]);
        return 1;
    }

    target_host = argv[1];
    if (argc >= 3) {
        max_hops = atoi(argv[2]);
    }
    if (argc >= 4) {
        dest_port = atoi(argv[3]);
    }

    if (resolve_ipv4(target_host, &dest_addr) != 0) {
        fprintf(stderr, "Failed to resolve %s\n", target_host);
        return 1;
    }
    dest_addr.sin_port = htons((unsigned short)dest_port);

    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    icmp_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (udp_sock < 0 || icmp_sock < 0) {
        perror("socket");
        fprintf(stderr, "Raw ICMP socket usually requires root/sudo privileges.\n");
        if (udp_sock >= 0) {
            close(udp_sock);
        }
        if (icmp_sock >= 0) {
            close(icmp_sock);
        }
        return 1;
    }

    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    if (setsockopt(icmp_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt");
        close(udp_sock);
        close(icmp_sock);
        return 1;
    }

    for (int ttl = 1; ttl <= max_hops; ttl++) {
        int destination_reached = 0;
        char hop_ip[INET_ADDRSTRLEN] = "";
        printf("%2d  ", ttl);

        if (setsockopt(udp_sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0) {
            perror("setsockopt TTL");
            break;
        }

        for (int probe = 0; probe < PROBES_PER_TTL; probe++) {
            char payload[MAX_PACKET_SIZE];
            char recv_buffer[1024];
            struct sockaddr_in responder_addr;
            socklen_t responder_len = sizeof(responder_addr);
            struct timespec start_ts;
            struct timespec end_ts;
            fd_set readfds;
            struct timeval probe_timeout;
            ssize_t sent_len;
            ssize_t recv_len;

            snprintf(payload, sizeof(payload), "ttl=%d probe=%d", ttl, probe + 1);
            clock_gettime(CLOCK_MONOTONIC, &start_ts);
            sent_len = sendto(
                udp_sock,
                payload,
                strlen(payload),
                0,
                (struct sockaddr *)&dest_addr,
                sizeof(dest_addr)
            );

            if (sent_len < 0) {
                printf("* ");
                continue;
            }

            FD_ZERO(&readfds);
            FD_SET(icmp_sock, &readfds);
            probe_timeout.tv_sec = 1;
            probe_timeout.tv_usec = 0;

            if (select(icmp_sock + 1, &readfds, NULL, NULL, &probe_timeout) <= 0) {
                printf("* ");
                continue;
            }

            recv_len = recvfrom(
                icmp_sock,
                recv_buffer,
                sizeof(recv_buffer),
                0,
                (struct sockaddr *)&responder_addr,
                &responder_len
            );
            clock_gettime(CLOCK_MONOTONIC, &end_ts);

            if (recv_len < (ssize_t)(sizeof(struct iphdr) + sizeof(struct icmphdr))) {
                printf("* ");
                continue;
            }

            {
                struct iphdr *ip_header = (struct iphdr *)recv_buffer;
                struct icmphdr *icmp_header = (struct icmphdr *)(recv_buffer + (ip_header->ihl * 4));
                double rtt_ms = time_diff_ms(&start_ts, &end_ts);

                inet_ntop(AF_INET, &responder_addr.sin_addr, hop_ip, sizeof(hop_ip));
                printf("%s  %.3f ms  ", hop_ip, rtt_ms);

                if (icmp_header->type == ICMP_DEST_UNREACH) {
                    destination_reached = 1;
                }
            }
        }

        printf("\n");
        if (destination_reached) {
            break;
        }
    }

    close(udp_sock);
    close(icmp_sock);
    return 0;
}
#else
#include <stdio.h>

int main(void) {
    fprintf(stderr, "This traceroute implementation targets Linux/POSIX because it uses raw ICMP sockets and netinet/ip_icmp.h.\n");
    fprintf(stderr, "Build and run it on Linux with sudo/root privileges.\n");
    return 1;
}
#endif
