# Network Troubleshooting Tools

## Overview

This repository contains the implementation of three socket-programming-based network troubleshooting tools developed for Part 2 of the assignment:

1. RTT measurement using a custom UDP echo client and server
2. Throughput and delay measurement using an iperf-like UDP echo workflow
3. Traceroute using UDP probes and ICMP responses

All three tools are implemented using low-level socket operations such as `socket()`, `bind()`, `setsockopt()`, `sendto()`, and `recvfrom()`, following the assignment requirement that the solution must be based on socket programming only and should not rely on higher-level wrappers.

The repository is organized by question so that each tool can be compiled, run, tested, and demonstrated independently.

## Objective

The main goal of this work is to understand how common network diagnostic tools are built internally by implementing simplified versions of:

- `ping`-like RTT measurement
- `iperf`-like throughput and delay measurement
- `traceroute`-like path discovery

Instead of using built-in system utilities, this submission recreates the essential behavior of these tools using custom socket programs.

## Core Idea Behind the Implementation

Each implemented tool is based on a fundamental networking principle:

- Q1 uses a UDP echo request and echo reply pair to estimate round-trip time.
- Q2 increases the packet sending rate of the UDP echo workflow and measures data returned per second to estimate throughput and average delay.
- Q3 controls the IP Time-To-Live value in UDP probes and listens for ICMP error messages to discover intermediate routers on the path to a destination.

Together, these programs demonstrate how transport-layer and network-layer behaviors can be observed and measured directly through socket programming.

## Implemented Tools And Functionalities

### Q1: RTT Measurement Tool

Files:

- `Q1_RTT/udp_echo_server.c`
- `Q1_RTT/udp_rtt_client.c`

Functionality:

- Creates a UDP echo server that receives packets and immediately sends them back to the sender
- Creates a UDP client that sends packets at user-specified intervals
- Stores sequence number and timestamp in each packet
- Measures RTT when the echoed packet returns
- Displays per-packet RTT
- Computes final packet loss percentage
- Computes minimum, average, and maximum RTT

### Q2: Throughput Measurement Tool

Files:

- `Q2_Throughput/udp_echo_server.c`
- `Q2_Throughput/udp_throughput_client.c`
- `Q2_Throughput/plot_results.py`

Functionality:

- Reuses the UDP echo server idea from Q1
- Sends UDP packets at a much higher rate by reducing the interval between packets
- Receives echoed packets and aggregates observations in 1-second buckets
- Calculates throughput in bits per second
- Calculates average delay for each 1-second interval
- Exports results to CSV
- Uses Python and Matplotlib to plot throughput vs time and delay vs time

### Q3: Traceroute Tool

Files:

- `Q3_Traceroute/traceroute_udp.c`

Functionality:

- Sends UDP probe packets with increasing TTL values
- Uses a raw socket to receive ICMP responses
- Sends three probes per TTL
- Measures RTT for each probe
- Displays `*` for timeouts
- Stops when the destination is reached or when the maximum hop count is exceeded
- Mimics the behavior of a standard traceroute utility in a simplified educational form

## Architecture

The overall architecture of the repository is modular. Each question is implemented in its own folder and each tool is split into clear sender/receiver responsibilities where applicable.

### High-Level Architecture

```text
                    +----------------------+
                    |   User Parameters    |
                    |  host, port, size,   |
                    | count, TTL, rate...  |
                    +----------+-----------+
                               |
                               v
                  +------------+-------------+
                  |   Tool-Specific Client   |
                  +------------+-------------+
                               |
                     UDP send / recv or
                    UDP send + ICMP receive
                               |
                               v
              +----------------+----------------+
              |      Remote Endpoint / Path     |
              | Echo server or network routers  |
              +----------------+----------------+
                               |
                               v
                   +-----------+-----------+
                   | Metrics / Observations|
                   | RTT, loss, throughput,|
                   | delay, hop addresses  |
                   +-----------------------+
```

### Per-Tool Architecture

#### Q1

- Client creates UDP socket
- Client sends echo packet with timestamp and sequence number
- Server receives packet on a bound UDP port
- Server sends the same packet back
- Client receives echoed packet and computes RTT

#### Q2

- Client sends UDP packets rapidly to the echo server
- Echo server returns packets
- Client collects replies in 1-second windows
- Throughput and average delay are computed for each time window
- Results are written to CSV for graph plotting

#### Q3

- UDP socket sends probes toward the destination
- TTL value is changed before each set of probes
- Raw ICMP socket listens for `Time Exceeded` or `Destination Unreachable`
- Each ICMP reply is matched to a sent probe at the current TTL
- Router IP and RTT are printed in traceroute-like format

## Design Decisions

Several practical design decisions were taken during implementation.

### 1. UDP Was Chosen For Q1 And Q2

UDP is connectionless and lightweight, making it suitable for direct RTT and throughput experiments without the additional behavior of TCP such as retransmission, congestion control, and connection establishment.

### 2. Custom Packet Header Was Embedded In The Payload

For RTT and throughput measurements, a small header containing:

- sequence number
- send timestamp

was placed at the beginning of each payload. This made it easy to compute RTT on packet reception.

### 3. Echo-Based Design Was Reused Across Q1 And Q2

The throughput measurement tool extends the RTT echo design instead of creating a new protocol from scratch. This kept the implementation simple and consistent with the assignment statement.

### 4. Non-Blocking Receive Was Used In Throughput Mode

Throughput mode needs to handle packets at a higher rate. Using a non-blocking receive path helps drain multiple echoed packets efficiently and allows throughput to be measured over time windows rather than waiting on a single request-reply pair.

### 5. Linux/POSIX Targeting For Traceroute

The traceroute implementation was intentionally written for Linux/POSIX because:

- the assignment hint references `netinet/ip_icmp.h`
- raw ICMP sockets generally require elevated privileges
- traceroute behavior is easier to demonstrate in a Linux environment

### 6. Separate Folders Per Question

The repository was reorganized so that each question has an isolated folder. This improves readability, presentation, and submission quality.

## Important Considerations

The following points were important while implementing the tools:

- Packet size must be large enough to hold the custom metadata header
- UDP does not guarantee delivery, ordering, or duplication protection
- Packet loss is therefore measured explicitly at the application level
- Throughput is measured from successfully echoed reply bytes observed at the client
- Raw ICMP sockets require administrative privileges
- Traceroute timeouts must be handled carefully so missing ICMP responses are displayed as `*`

## Folder Structure

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

## Tech Stack

Languages and tools used:

- C for the networking tools
- Python for plotting Q2 results
- GCC for compilation
- Winsock2 for Windows-compatible UDP programs
- POSIX sockets and raw ICMP sockets for Linux traceroute
- Matplotlib for graph generation

## Implementation Details

### Q1 Implementation Details

- UDP echo server binds to a user-provided or default port
- Server uses a fixed-size receive buffer and sends back the received bytes
- Client accepts command-line arguments for host, port, count, interval, and packet size
- Client timestamps the packet before sending
- RTT is calculated as:

`RTT = receive_timestamp - send_timestamp`

- Final statistics include:
  sent count, received count, lost count, loss percentage, min RTT, avg RTT, max RTT

### Q2 Implementation Details

- Client accepts parameters for duration, packet size, and rate in packets per second
- Packet transmission is controlled using a computed inter-send gap
- Echo replies are grouped in 1-second buckets
- Per-second throughput is computed as:

`throughput = bytes_received_in_one_second * 8`

- Per-second average delay is computed from all replies received in the same interval
- Results are exported as CSV so they can be plotted later

### Q3 Implementation Details

- Each probe is sent through a UDP socket
- `setsockopt()` is used to modify the TTL value before each probe set
- A raw ICMP socket listens for router responses
- `select()` and socket timeout handling are used to avoid indefinite waiting
- The program prints one line per TTL and three observations per hop
- It stops when an ICMP destination unreachable message indicates the target host has been reached

## Setup And Build Instructions

### Prerequisites

- GCC compiler
- Python 3
- Matplotlib installed for graph plotting
- Linux environment for Q3 traceroute execution

### Build Commands

#### Windows For Q1 And Q2

```powershell
cd Q1_RTT
gcc udp_echo_server.c -o udp_echo_server.exe -lws2_32
gcc udp_rtt_client.c -o udp_rtt_client.exe -lws2_32

cd ..\Q2_Throughput
gcc udp_echo_server.c -o udp_echo_server.exe -lws2_32
gcc udp_throughput_client.c -o udp_throughput_client.exe -lws2_32
```

#### Linux For Q1, Q2, And Q3

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

## How To Run

### Q1 RTT Tool

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

### Q2 Throughput Tool

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
- `csv_path` optional

### Q3 Traceroute Tool

```bash
cd Q3_Traceroute
gcc traceroute_udp.c -o traceroute_udp
sudo ./traceroute_udp 8.8.8.8 30 33434
```

Arguments:

- `destination`
- `max_hops` optional, default `30`
- `dest_port` optional, default `33434`

## Results

### Q1 Observations

The RTT measurement tool successfully reports:

- individual packet RTT
- final min, average, and max RTT
- packet loss percentage

In local testing on `127.0.0.1`, the observed RTT values were very small, which is expected because client and server were running on the same machine.

### Q2 Observations

The throughput tool successfully reports:

- per-second throughput
- per-second average delay
- final packet loss percentage

In local testing, the tool generated stable throughput values and CSV output suitable for graph plotting.

### Q3 Expected Observations

When run on Linux with proper privileges, the traceroute tool is expected to:

- print one row for each TTL
- display router IPs for intermediate hops
- show RTT for each probe
- display `*` where no ICMP response is received

Actual results depend on the target destination, local routing policy, firewall behavior, and network permissions.

## Terminal Output

- Q1 server startup and RTT client output
- Q2 throughput client output and generated graph
- Q3 traceroute execution on Linux with `sudo`

## Challenges And Solutions

### Challenge 1: Measuring RTT Reliably

Problem:

- Each echoed packet must be matched with the correct send event

Solution:

- A custom packet header with sequence number and timestamp was inserted into the payload

### Challenge 2: Handling High Packet Rates In Throughput Mode

Problem:

- A blocking request-reply model reduces throughput and can distort measurements

Solution:

- The throughput client was designed to send rapidly and drain replies efficiently using non-blocking receive behavior

### Challenge 3: Implementing Traceroute Without Using Existing Utilities

Problem:

- Traceroute depends on network-layer behavior, not just simple application-layer messaging

Solution:

- TTL-controlled UDP probes and a raw ICMP receive socket were used to recreate the essential logic

### Challenge 4: Timeout Representation

Problem:

- Missing replies should not freeze the program or produce unclear output

Solution:

- Socket timeout and `select()`-based waiting were used so missing packets are displayed as `*`

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

## Conclusion

This project demonstrates how three important network diagnostic utilities can be implemented from scratch using socket programming principles. The RTT tool shows how latency and packet loss can be measured using UDP echo behavior. The throughput tool extends that design to observe bandwidth-related behavior over time. The traceroute tool shows how TTL and ICMP interactions reveal the path taken by packets through the network.

Overall, this work provides a practical understanding of network measurement techniques and highlights the internal mechanisms behind commonly used troubleshooting tools.
