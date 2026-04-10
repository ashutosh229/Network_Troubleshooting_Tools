#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef int socklen_type;
#define CLOSESOCKET closesocket
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
typedef socklen_t socklen_type;
#define CLOSESOCKET close
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
typedef int SOCKET;
#endif

typedef struct {
    uint32_t seq;
    uint64_t send_time_us;
} PacketHeader;

typedef struct {
    uint64_t second_index;
    uint64_t bytes_received;
    double delay_sum_ms;
    uint32_t replies;
} ThroughputBucket;

static uint64_t now_us(void) {
#ifdef _WIN32
    FILETIME ft;
    ULARGE_INTEGER uli;
    GetSystemTimePreciseAsFileTime(&ft);
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    return (uli.QuadPart - 116444736000000000ULL) / 10ULL;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec * 1000000ULL) + ((uint64_t)ts.tv_nsec / 1000ULL);
#endif
}

static void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec req;
    req.tv_sec = ms / 1000;
    req.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&req, NULL);
#endif
}

static void cleanup_and_exit(SOCKET sock, int code) {
    if (sock != INVALID_SOCKET) {
        CLOSESOCKET(sock);
    }
#ifdef _WIN32
    WSACleanup();
#endif
    exit(code);
}

static void print_usage(const char *program) {
    fprintf(stderr,
            "Usage:\n"
            "  %s rtt <host> <port> <count> <interval_ms> <packet_size>\n"
            "  %s throughput <host> <port> <duration_s> <packet_size> <rate_pps> [csv_path]\n",
            program, program);
}

static int resolve_address(const char *host, int port, struct sockaddr_in *out_addr) {
    char port_str[16];
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *rp = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &result) != 0) {
        return -1;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        if (rp->ai_addrlen == sizeof(struct sockaddr_in)) {
            memcpy(out_addr, rp->ai_addr, sizeof(struct sockaddr_in));
            freeaddrinfo(result);
            return 0;
        }
    }

    freeaddrinfo(result);
    return -1;
}

static int set_recv_timeout(SOCKET sock, int timeout_ms) {
#ifdef _WIN32
    DWORD timeout = (DWORD)timeout_ms;
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    return setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

static int set_nonblocking(SOCKET sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
}

static int run_rtt_mode(SOCKET sock, struct sockaddr_in *server_addr, int count, int interval_ms, int packet_size) {
    int sent = 0;
    int received = 0;
    double rtt_sum_ms = 0.0;
    double rtt_min_ms = 0.0;
    double rtt_max_ms = 0.0;
    char *send_buffer = NULL;
    char *recv_buffer = NULL;
    socklen_type addr_len = (socklen_type)sizeof(*server_addr);

    if (packet_size < (int)sizeof(PacketHeader)) {
        fprintf(stderr, "Packet size must be at least %zu bytes\n", sizeof(PacketHeader));
        return 1;
    }

    send_buffer = (char *)malloc((size_t)packet_size);
    recv_buffer = (char *)malloc((size_t)packet_size);
    if (send_buffer == NULL || recv_buffer == NULL) {
        fprintf(stderr, "Failed to allocate packet buffers\n");
        free(send_buffer);
        free(recv_buffer);
        return 1;
    }

    for (int i = 0; i < count; i++) {
        PacketHeader header;
        int n;
        uint64_t recv_time_us;
        double rtt_ms;

        memset(send_buffer, 'A' + (i % 26), (size_t)packet_size);
        header.seq = htonl((uint32_t)(i + 1));
        header.send_time_us = (uint64_t)now_us();
        memcpy(send_buffer, &header, sizeof(header));

        sent++;
        if (sendto(sock, send_buffer, packet_size, 0, (struct sockaddr *)server_addr, addr_len) == SOCKET_ERROR) {
            printf("seq=%d request send failed\n", i + 1);
            sleep_ms(interval_ms);
            continue;
        }

        n = recvfrom(sock, recv_buffer, packet_size, 0, NULL, NULL);
        if (n == SOCKET_ERROR) {
            printf("seq=%d timeout\n", i + 1);
            sleep_ms(interval_ms);
            continue;
        }

        recv_time_us = now_us();
        memcpy(&header, recv_buffer, sizeof(header));
        rtt_ms = (double)(recv_time_us - header.send_time_us) / 1000.0;
        received++;
        rtt_sum_ms += rtt_ms;
        if (received == 1 || rtt_ms < rtt_min_ms) {
            rtt_min_ms = rtt_ms;
        }
        if (received == 1 || rtt_ms > rtt_max_ms) {
            rtt_max_ms = rtt_ms;
        }

        printf("seq=%" PRIu32 " bytes=%d rtt=%.3f ms\n", ntohl(header.seq), n, rtt_ms);
        sleep_ms(interval_ms);
    }

    printf("\n--- RTT Summary ---\n");
    printf("Packets: sent=%d received=%d lost=%d loss=%.2f%%\n",
           sent,
           received,
           sent - received,
           sent == 0 ? 0.0 : ((double)(sent - received) * 100.0 / (double)sent));
    if (received > 0) {
        printf("RTT (ms): min=%.3f avg=%.3f max=%.3f\n",
               rtt_min_ms,
               rtt_sum_ms / (double)received,
               rtt_max_ms);
    }

    free(send_buffer);
    free(recv_buffer);
    return 0;
}

static int run_throughput_mode(
    SOCKET sock,
    struct sockaddr_in *server_addr,
    int duration_s,
    int packet_size,
    int rate_pps,
    const char *csv_path
) {
    uint64_t start_us;
    uint64_t send_end_us;
    uint64_t drain_end_us;
    uint64_t next_send_us;
    uint64_t send_gap_us;
    uint32_t seq = 1;
    uint64_t sent = 0;
    uint64_t received = 0;
    uint64_t lost = 0;
    char *send_buffer = NULL;
    char *recv_buffer = NULL;
    FILE *csv = NULL;
    ThroughputBucket *buckets = NULL;
    socklen_type addr_len = (socklen_type)sizeof(*server_addr);

    if (packet_size < (int)sizeof(PacketHeader)) {
        fprintf(stderr, "Packet size must be at least %zu bytes\n", sizeof(PacketHeader));
        return 1;
    }
    if (duration_s <= 0 || rate_pps <= 0) {
        fprintf(stderr, "Duration and rate must be positive\n");
        return 1;
    }

    send_buffer = (char *)malloc((size_t)packet_size);
    recv_buffer = (char *)malloc((size_t)packet_size);
    buckets = (ThroughputBucket *)calloc((size_t)duration_s, sizeof(ThroughputBucket));
    if (send_buffer == NULL || recv_buffer == NULL || buckets == NULL) {
        fprintf(stderr, "Failed to allocate buffers\n");
        free(send_buffer);
        free(recv_buffer);
        free(buckets);
        return 1;
    }

    if (csv_path != NULL) {
        csv = fopen(csv_path, "w");
        if (csv == NULL) {
            fprintf(stderr, "Failed to open CSV file: %s\n", csv_path);
            free(send_buffer);
            free(recv_buffer);
            free(buckets);
            return 1;
        }
        fprintf(csv, "second,throughput_bps,avg_delay_ms,replies\n");
    }

    send_gap_us = 1000000ULL / (uint64_t)rate_pps;
    if (send_gap_us == 0) {
        send_gap_us = 1;
    }

    start_us = now_us();
    send_end_us = start_us + ((uint64_t)duration_s * 1000000ULL);
    drain_end_us = send_end_us + 500000ULL;
    next_send_us = start_us;

    while (now_us() < drain_end_us) {
        uint64_t current_us = now_us();

        while (current_us >= next_send_us && next_send_us < send_end_us) {
            PacketHeader header;

            memset(send_buffer, 'a' + (seq % 26), (size_t)packet_size);
            header.seq = htonl(seq);
            header.send_time_us = current_us;
            memcpy(send_buffer, &header, sizeof(header));

            if (sendto(sock, send_buffer, packet_size, 0, (struct sockaddr *)server_addr, addr_len) != SOCKET_ERROR) {
                sent++;
            }

            seq++;
            next_send_us += send_gap_us;
        }

        for (;;) {
            PacketHeader header;
            int n = recvfrom(sock, recv_buffer, packet_size, 0, NULL, NULL);
            if (n == SOCKET_ERROR) {
                break;
            }

            {
                ThroughputBucket *bucket;
                uint64_t recv_time_us = now_us();
                double rtt_ms;
                uint64_t second_index;

                memcpy(&header, recv_buffer, sizeof(header));
                rtt_ms = (double)(recv_time_us - header.send_time_us) / 1000.0;
                second_index = (recv_time_us - start_us) / 1000000ULL;
                if (second_index >= (uint64_t)duration_s) {
                    second_index = (uint64_t)duration_s - 1ULL;
                }

                bucket = &buckets[second_index];
                bucket->second_index = second_index + 1ULL;
                bucket->bytes_received += (uint64_t)n;
                bucket->delay_sum_ms += rtt_ms;
                bucket->replies += 1U;
                received++;
            }
        }

        sleep_ms(1);
    }

    lost = sent - received;

    printf("second\tthroughput(bps)\tavg_delay(ms)\treplies\n");
    for (int i = 0; i < duration_s; i++) {
        double throughput_bps = (double)buckets[i].bytes_received * 8.0;
        double avg_delay_ms = buckets[i].replies == 0 ? 0.0 : buckets[i].delay_sum_ms / (double)buckets[i].replies;
        printf("%d\t%.2f\t\t%.3f\t\t%u\n", i + 1, throughput_bps, avg_delay_ms, buckets[i].replies);
        if (csv != NULL) {
            fprintf(csv, "%d,%.2f,%.3f,%u\n", i + 1, throughput_bps, avg_delay_ms, buckets[i].replies);
        }
    }

    printf("\n--- Throughput Summary ---\n");
    printf("Packets: sent=%" PRIu64 " received=%" PRIu64 " lost=%" PRIu64 " loss=%.2f%%\n",
           sent,
           received,
           lost,
           sent == 0 ? 0.0 : ((double)lost * 100.0 / (double)sent));

    if (csv != NULL) {
        fclose(csv);
    }
    free(send_buffer);
    free(recv_buffer);
    free(buckets);
    return 0;
}

int main(int argc, char *argv[]) {
    SOCKET sock = INVALID_SOCKET;
    struct sockaddr_in server_addr;
    const char *mode;
    int port;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    mode = argv[1];
    memset(&server_addr, 0, sizeof(server_addr));

    if (strcmp(mode, "rtt") == 0) {
        if (argc != 7) {
            print_usage(argv[0]);
            cleanup_and_exit(sock, 1);
        }

        port = atoi(argv[3]);
        if (resolve_address(argv[2], port, &server_addr) != 0) {
            fprintf(stderr, "Failed to resolve host %s\n", argv[2]);
            cleanup_and_exit(sock, 1);
        }

        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == INVALID_SOCKET) {
            fprintf(stderr, "Failed to create UDP socket\n");
            cleanup_and_exit(sock, 1);
        }

        if (set_recv_timeout(sock, atoi(argv[5])) == SOCKET_ERROR) {
            fprintf(stderr, "Failed to set receive timeout\n");
            cleanup_and_exit(sock, 1);
        }

        {
            int count = atoi(argv[4]);
            int interval_ms = atoi(argv[5]);
            int packet_size = atoi(argv[6]);
            int rc = run_rtt_mode(sock, &server_addr, count, interval_ms, packet_size);
            cleanup_and_exit(sock, rc);
        }
    } else if (strcmp(mode, "throughput") == 0) {
        if (argc != 7 && argc != 8) {
            print_usage(argv[0]);
            cleanup_and_exit(sock, 1);
        }

        port = atoi(argv[3]);
        if (resolve_address(argv[2], port, &server_addr) != 0) {
            fprintf(stderr, "Failed to resolve host %s\n", argv[2]);
            cleanup_and_exit(sock, 1);
        }

        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == INVALID_SOCKET) {
            fprintf(stderr, "Failed to create UDP socket\n");
            cleanup_and_exit(sock, 1);
        }

        if (set_recv_timeout(sock, 200) == SOCKET_ERROR) {
            fprintf(stderr, "Failed to set receive timeout\n");
            cleanup_and_exit(sock, 1);
        }
        if (set_nonblocking(sock) == SOCKET_ERROR) {
            fprintf(stderr, "Failed to set nonblocking mode\n");
            cleanup_and_exit(sock, 1);
        }

        {
            int duration_s = atoi(argv[4]);
            int packet_size = atoi(argv[5]);
            int rate_pps = atoi(argv[6]);
            const char *csv_path = argc == 8 ? argv[7] : "throughput_results.csv";
            int rc = run_throughput_mode(sock, &server_addr, duration_s, packet_size, rate_pps, csv_path);
            cleanup_and_exit(sock, rc);
        }
    } else {
        print_usage(argv[0]);
        cleanup_and_exit(sock, 1);
    }

    return 0;
}
