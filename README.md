# Network Troubleshooting Tools

This repository is organized by assignment question so each part is easy to build and present separately.

## Repository Structure

```text
Network_Troubleshooting_Tools/
|-- Q1_RTT/
|   |-- udp_echo_server.c
|   `-- udp_rtt_client.c
|-- Q2_Throughput/
|   |-- udp_echo_server.c
|   |-- udp_throughput_client.c
|   `-- plot_results.py
|-- Q3_Traceroute/
|   `-- traceroute_udp.c
`-- README.md
```

## Q1: RTT Measurement

Files:

- `Q1_RTT/udp_echo_server.c`
- `Q1_RTT/udp_rtt_client.c`

The client sends UDP echo packets to the server. Each packet contains:

- A sequence number
- A timestamp captured at send time

When the echoed packet returns, the client computes RTT as:

`RTT = receive_time - send_time`

At the end, it prints:

- Sent packets
- Received packets
- Lost packets
- Loss percentage
- Min/avg/max RTT

## Q2: Throughput Measurement

Files:

- `Q2_Throughput/udp_echo_server.c`
- `Q2_Throughput/udp_throughput_client.c`
- `Q2_Throughput/plot_results.py`

The client in this folder uses the same UDP echo idea as Q1, but runs in `throughput` mode. It:

- Sends UDP packets at a higher rate by reducing the inter-packet gap
- Measures received bytes per 1-second interval
- Computes per-second throughput in bits per second
- Computes average RTT for replies received in each 1-second interval
- Writes the observed values to a CSV file for plotting

## Q3: Traceroute

Files:

- `Q3_Traceroute/traceroute_udp.c`

The traceroute implementation:

- Sends UDP probes with increasing TTL values
- Receives ICMP responses through a raw socket
- Sends three probes per TTL
- Measures RTT for each probe
- Prints `*` on timeout
- Stops when the destination is reached or max hop count is exceeded

`traceroute_udp.c` is intended for Linux/POSIX because the assignment hint refers to `netinet/ip_icmp.h` and `sudo`.

## Build

### Windows (Q1 and Q2)

```powershell
cd Q1_RTT
gcc udp_echo_server.c -o udp_echo_server.exe -lws2_32
gcc udp_rtt_client.c -o udp_rtt_client.exe -lws2_32

cd ..\Q2_Throughput
gcc udp_echo_server.c -o udp_echo_server.exe -lws2_32
gcc udp_throughput_client.c -o udp_throughput_client.exe -lws2_32
```

### Linux (Q1, Q2, and Q3)

```bash
cd Q1_RTT
gcc udp_echo_server.c -o udp_echo_server
gcc udp_rtt_client.c -o udp_rtt_client

cd ../Q2_Throughput
gcc udp_echo_server.c -o udp_echo_server
gcc udp_throughput_client.c -o udp_throughput_client

cd ../Q3_Traceroute
gcc traceroute_udp.c -o traceroute_udp
```

## Run

### Q1 RTT mode

```powershell
cd Q1_RTT
./udp_echo_server.exe 9000 1024
./udp_rtt_client.exe rtt 127.0.0.1 9000 10 1000 128
```

Arguments:

- `host`
- `port`
- `count`
- `interval_ms`
- `packet_size`

### Q2 throughput mode

```powershell
cd Q2_Throughput
./udp_echo_server.exe 9000 1024
./udp_throughput_client.exe throughput 127.0.0.1 9000 10 512 200 throughput_results.csv
python plot_results.py throughput_results.csv
```

Arguments:

- `host`
- `port`
- `duration_s`
- `packet_size`
- `rate_pps`
- `csv_path` (optional)

### Q3 traceroute

```bash
cd Q3_Traceroute
sudo ./traceroute_udp 8.8.8.8 30 33434
```

Arguments:

- `destination`
- `max_hops` (optional, default `30`)
- `dest_port` (optional, default `33434`)

## Notes

- Packet size must be at least large enough to hold the internal header used by the client.
- The throughput numbers are based on successfully echoed reply bytes observed by the client in each 1-second bucket.
- For public-network traceroute tests, run on Linux with sudo/root privileges.

## References

1. Beej's Guide to Network Programming
   https://beej.us/guide/bgnet/

2. Linux `traceroute` manual page
   https://man7.org/linux/man-pages/man8/traceroute.8.html

3. Linux `icmp(7)` manual page
   https://man7.org/linux/man-pages/man7/icmp.7.html

4. Linux `ip(7)` manual page
   https://man7.org/linux/man-pages/man7/ip.7.html

5. iPerf3 documentation
   https://software.es.net/iperf/

6. Microsoft Winsock documentation
   https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-start-page
