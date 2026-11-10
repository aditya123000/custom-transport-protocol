# Reliable UDP Learning Project

## Working agreement

Build one stage at a time. This file records the code, reasoning, important
lines, verification instructions, and suggested experiments so the chat does
not need to repeat them.

## Stage 1 — basic UDP file transfer

Implemented files:

- `backend/CMakeLists.txt`
- `backend/src/client.cpp`
- `backend/src/server.cpp`
- `backend/src/network/udp_socket.h`
- `backend/src/network/udp_socket.cpp`

The client reads an input file in 1024-byte blocks and uses `sendto` to make
each block one UDP datagram. The server receives with `recvfrom` and writes
each datagram in arrival order. The client sends a final zero-byte datagram;
the server treats it as the temporary end-of-file marker.

The important Stage 1 lines are `sendto(...)`, which sends an independent,
unconnected datagram, `output.write(...)`, which trusts arrival order, and
`if (received == 0) break`, which gives the server a stopping condition.
Without the first, no data leaves the client; without the second, received
bytes never reach the output file; without the last, the server waits forever.

Build and run on Linux:

```bash
cd backend
cmake -S . -B build
cmake --build build

./build/udp_server 40000 output.txt
# In a second terminal:
./build/udp_client 127.0.0.1 40000 input.txt
diff -u input.txt output.txt
```

This may work on localhost but is not reliable: UDP may drop, duplicate, or
reorder datagrams, and this version has no way to detect or repair any of it.

Suggested experiment: send a file larger than 100 KB, then run the client
twice against one server. The output becomes approximately two concatenated
copies, showing there is no connection identity or duplicate detection.

Verification status: a local Linux build could not be run from this Windows
environment because the installed WSL service returned an access-denied error.
Run the commands above in a Linux shell to perform the acceptance test.

## Stage 2 — custom packet header

Implemented next. Each UDP payload now begins with a 13-byte explicitly
serialized protocol header:

```text
seq_num(4) | ack_num(4) | window_size(2) | payload_size(2) | flags(1)
```

The wire format is explicit rather than sending a C++ `struct` directly:
struct padding and native byte order are not a network protocol. All
multi-byte fields use network byte order (`htonl`/`htons` when writing and
`ntohl`/`ntohs` when reading), so peers on different CPU architectures agree
on the values.

For now, `seq_num`, `ack_num`, and `window_size` are zero; sequence management
belongs to Stage 3. Normal chunks have the `DATA` flag. The final zero-payload
packet has the `FIN` flag, replacing Stage 1's untyped empty datagram.

The receiver parses and validates every packet before writing its payload, and
logs the parsed fields. It stops only after a valid zero-payload `FIN` packet.

Important Stage 2 lines:

- `htonl` / `htons` in `serialize_packet`: removing these makes the protocol
  host-endian and therefore incorrect across architectures.
- `payload_size != received_size - kHeaderSize` in the parser: removing this
  allows malformed or truncated datagrams to be accepted and can make the
  server write unintended bytes.
- `if ((packet.header.flags & rudp::FIN) != 0)`: removing it leaves the server
  without a protocol-level completion signal.

Build/run commands remain the same as Stage 1. Acceptance check: transfer a
small text file and confirm the server logs `DATA` packet(s) with matching
payload sizes followed by a `FIN` packet with payload size `0`; then confirm
`diff -u input.txt output.txt` produces no output.

Suggested experiment: temporarily replace `rudp::htons(header.payload_size)`
with `header.payload_size`, send a 300-byte file, and inspect the logged
payload size. On a typical little-endian machine it becomes a nonsensical
value (often 11265), demonstrating why byte order is part of the protocol.

## Stage 3 — reliability: sequence numbers, cumulative ACKs, retransmission

Implemented as stop-and-wait reliability first: only one packet is in flight.
`ReliableSender` assigns packet sequence numbers beginning at 1 and does not
move to the next packet until it receives an `ACK` whose `ack_num` is one more
than the sent sequence number. The receiver tracks its next expected sequence
number. It delivers only that exact packet, then sends its new expected value
as a cumulative ACK. Duplicates and out-of-order packets are discarded but
still answered with the current cumulative ACK.

This deliberately makes Stage 4 straightforward: its sliding window will
allow multiple copies of the same stop-and-wait state machine to be in flight.

`RtoEstimator` implements Jacobson's adaptive timeout:

```text
first sample: SRTT = RTT, RTTVAR = RTT / 2
later:        RTTVAR = .75 * RTTVAR + .25 * |SRTT - RTT|
              SRTT   = .875 * SRTT + .125 * RTT
              RTO    = clamp(SRTT + 4 * RTTVAR, 100 ms, 60 s)
```

On timeout, RTO doubles. `ReliableSender::send` records an RTT sample only if
that packet was never retransmitted: this is Karn's algorithm, which avoids
mistaking an ACK for a retransmission as an ACK for the first transmission.

Key conceptual lines:

- `header.seq_num == expected_sequence_` in `ReliableReceiver::receive` is
  the delivery gate. Removing it lets duplicate and out-of-order packets be
  written, corrupting the file.
- `ack_num != header.seq_num + 1` in `ReliableSender::send` prevents an old or
  duplicate ACK from acknowledging the wrong packet.
- `if (!retransmitted)` around `record_rtt(...)` is Karn's algorithm. Removing
  it lets ambiguous samples shrink RTO incorrectly and causes spurious future
  retransmissions.

To test loss on Linux (run as root, and replace `eth0` with the loopback or
test interface appropriate to your setup):

```bash
sudo tc qdisc add dev eth0 root netem loss 2%
./build/udp_server 40000 output.txt
./build/udp_client 127.0.0.1 40000 input.txt
diff -u input.txt output.txt
sudo tc qdisc del dev eth0 root
```

The client prints timeout/retransmission events and a final retransmission
count. The transfer should complete and `diff` should be empty despite loss.
Suggested experiment: run a 1–5 MB transfer at 2% loss and compare the
printed retransmission count against a no-loss run. Then use `tc qdisc change`
to add 100 ms delay and observe the adaptive RTO settle above the RTT.

## Stage 4 — sliding window and flow control

The sender now keeps up to eight packets in flight. Every outstanding packet
has an individual send time and retransmission marker. An ACK for `N` is
cumulative: it removes all outstanding packet numbers below `N`.

The receiver holds up to eight ahead-of-time packets in a reorder buffer. Its
ACK advertises the remaining slots, and the sender uses the smaller of its
local window and that advertised window. When a missing packet arrives, the
receiver releases that packet plus all newly contiguous buffered packets in
order.

The key lines are `outstanding.size() < usable_window` (the flow-control gate),
the ACK removal condition `sequence < ack_num` (cumulative ACK semantics), and
the receiver's reorder-buffer loop (the ordering guarantee). Removing any one
respectively permits unlimited sending, leaves acknowledged packets in flight,
or strands buffered packets forever.

Acceptance test: apply 100 ms artificial delay, send the same input with
`kLocalWindow` temporarily set to `1`, then again at `8`. The window-eight run
should finish substantially faster because it can occupy more of the
bandwidth-delay product. With 1% loss added, the output must still match.

```bash
sudo tc qdisc add dev eth0 root netem delay 100ms loss 1%
time ./build/udp_client 127.0.0.1 40000 input.txt
diff -u input.txt output.txt
sudo tc qdisc del dev eth0 root
```

Suggested experiment: watch `advertised_window` and `in_flight` in the client
output while loss and delay are enabled. They expose the sender/receiver
feedback loop directly.
