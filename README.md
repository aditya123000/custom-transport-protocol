# Custom Reliable UDP Transport Protocol

A learning project that transfers a file over UDP using a small custom protocol. It adds sequence numbers, cumulative acknowledgements, retransmission, adaptive timeouts, a bounded sliding window, and receiver-side reordering on top of UDP.

> This is an educational prototype, not a production transport protocol. Use it on trusted test networks only.

## Requirements

- Linux or another POSIX system with Berkeley sockets (`socket`, `sendto`, `recvfrom`, `poll`).
- CMake 3.16 or newer.
- A C++17 compiler (for example, GCC or Clang).

The implementation includes POSIX headers such as `<unistd.h>` and is not currently a native Windows build.

## Build

From the repository root:

```sh
cmake -S backend -B build
cmake --build build
```

The executables are created at `build/udp_server` and `build/udp_client`.

## Transfer a file

Start the receiver first:

```sh
./build/udp_server 40000 received.bin
```

In another terminal, send a file to its IPv4 address:

```sh
./build/udp_client 127.0.0.1 40000 input.bin
```

The server binds to all local IPv4 interfaces on the selected port and truncates/creates the output file at startup. The client accepts an IPv4 literal (DNS names are not resolved). Both programs print usage when invoked with the wrong number of arguments. The server exits after receiving the ordered FIN packet; run a fresh server for each transfer.

To compare binary files on Linux:

```sh
cmp input.bin received.bin
```

A successful comparison exits with no output. The client reports its retransmission count; both ends print packet/ACK progress.

## Repository structure

```text
backend/
  CMakeLists.txt             Build definition and executable targets
  src/
    client.cpp               Reads input, chunks it, adds FIN, invokes sender
    server.cpp               Receives datagrams, ACKs, writes delivered bytes
    network/
      udp_socket.h/.cpp      IPv4 UDP socket creation, binding, and sending
    protocol/
      packet.h               Header fields, flag values, wire-size constants
      serializer.h/.cpp      Encodes packet headers and payloads
      parser.h/.cpp          Decodes and validates received packets
    reliability/
      sender.h/.cpp           Sliding-window sender, ACK handling, retransmits
      receiver.h/.cpp         Sequence tracking and out-of-order buffering
      retransmission.h/.cpp   RTT estimator and timeout backoff
PROJECT_NOTES.md             Learning notes, rationale, and experiments
.gitignore                   Excludes local build and generated files
```

## How the protocol works

Each UDP datagram contains a 13-byte header followed by up to 1024 payload bytes. The header is serialized field by field; it does not depend on C++ struct padding.

| Field | Size | Meaning |
|---|---:|---|
| `seq_num` | 4 bytes | Packet sequence number; data and FIN start at 1 |
| `ack_num` | 4 bytes | Cumulative next sequence number expected by receiver |
| `window_size` | 2 bytes | Receiver's currently advertised buffer capacity |
| `payload_size` | 2 bytes | Number of payload bytes following the header |
| `flags` | 1 byte | Packet type bits: `SYN`, `ACK`, `FIN`, `DATA` |

All multi-byte header values use network byte order. The parser checks that the datagram is at least header-sized and that declared payload length matches the actual datagram, with a maximum payload of 1024 bytes.

### Sender

1. The client reads the entire input into 1024-byte DATA packets and appends a zero-payload FIN packet.
2. The sender transmits up to eight packets at a time, limited also by the receiver's advertised window.
3. ACKs are cumulative: an ACK number removes all outstanding packets with lower sequence numbers.
4. Outstanding packets are retransmitted after timeout. The timeout estimator follows the Jacobson/Karels RTT equations, clamps RTO between 100 ms and 60 s, and doubles RTO after timeout. RTT samples are ignored for retransmitted packets (Karn's algorithm).

### Receiver

The receiver expects sequence 1, delivers in-order packets, and buffers packets that arrive ahead of a gap within its eight-packet reorder window. It acknowledges the next sequence expected and advertises remaining buffer slots. Duplicate, stale, or too-far-ahead packets are not delivered. Once the missing packet arrives, buffered contiguous packets are delivered in sequence. FIN ends the transfer only when it is delivered in order.

## Current scope and limitations

- IPv4 numeric addresses only; no DNS lookup, handshake, connection identity, or concurrent transfer isolation.
- The server accepts datagrams from any sender on its bound port and maintains one receiver state. Keep it on a trusted network.
- No authentication, encryption, congestion control, path MTU discovery, or robust peer validation. The fixed 1024-byte payload is intentionally conservative for common MTUs, but is not negotiated.
- No explicit transfer identifier: a second concurrent client or delayed datagrams can interfere with the one server session.
- The sender loads the whole input file into memory before transmission.
- The protocol is a teaching implementation and should not be used as a replacement for TCP, QUIC, or a production transport.

## Learning notes

[PROJECT_NOTES.md](PROJECT_NOTES.md) describes the progression from basic UDP transfer through packet framing, reliability, and sliding-window flow control, including rationale and suggested experiments.
