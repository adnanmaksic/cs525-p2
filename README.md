# P2: Go-Back-N Reliable Transfer over UDP

- Name: Adnan Maksic
- Email: adnan.maksic06@gmail.com
- Class: CS525

## Overview

`myapp` moves a file reliably over UDP through the course relay, which drops,
corrupts and duplicates datagrams on purpose. Reliability is built on top with
an RFC 1071 checksum, per-packet sequence numbers, cumulative ACKs, one
retransmission timer and a Go-Back-N sliding window.

```bash
make all
python3 cs425_relay.py --delay 50
./build/release/myapp recv -s jdoe-1 127.0.0.1 out.bin
./build/release/myapp send -s jdoe-1 -w 16 -l 0.1 -c 0.05 127.0.0.1 in.bin
cmp in.bin out.bin && echo identical
```

Exit codes: 0 on a successful transfer (and when run with no arguments, which
prints the usage), 1 for a bad command line, 2 when the relay refuses us, the
network fails, or the transfer gives up.

## Design

The program is split into three layers so that the protocol itself can be
tested without a network, a clock or a disk.

1. **Packets** (`src/lab.h`, `src/lab.c`). `inet_checksum`, `pkt_encode` and
   `pkt_decode` are pure functions: bytes in, bytes or a `struct packet` out.
   Header fields are written byte by byte in network order with
   `htons`/`htonl`, never by copying a struct onto the wire. `pkt_decode`
   checks the datagram size and the length field against each other *before*
   copying any payload, then the type, the reserved byte and the checksum, so
   a flipped bit anywhere, including in the length field, is just a lost
   packet.

2. **Go-Back-N state machines** (`src/lab.h`, `src/lab.c`). `struct
   gbn_sender` holds `base`, `next`, a ring of encoded copies of the in-flight
   packets (slot `seq % window`), one timer deadline, and a count of
   timeouts in a row. `struct gbn_receiver` holds `expected` plus its linger
   and idle deadlines. They are driven by events: `gbn_sender_pump`,
   `gbn_sender_on_packet`, `gbn_sender_on_tick`, `gbn_receiver_on_packet` and
   `gbn_receiver_on_tick`. Every event takes the current time in milliseconds
   as a parameter and answers with what to do: a list of datagrams to send
   (`struct gbn_tx`), or an ACK to send, a payload to append and whether the
   file is complete (`struct gbn_rx`). `gbn_*_wait_ms` says how long the
   caller may sleep. Nothing in this layer calls `sendto`, `recv`, `poll`,
   `clock_gettime` or `fwrite`.

3. **I/O** (`src/main.c`). Command line parsing with `getopt`, `getaddrinfo`,
   one connected UDP socket for the whole run, the relay hello with five
   one-second tries, the `poll` loops, `clock_gettime(CLOCK_MONOTONIC)` and the
   files. Each loop is a few lines: wait for a datagram or the timer, decode,
   hand the event to the state machine, send what it returns.

**Why.** A relay that loses packets at random cannot be unit tested, but a
state machine that takes "now" as a number can. In `tests/lab-test.c` a lost
packet is a datagram the test does not pass along and a timeout is a larger
value of `now`, so every rule (window full, cumulative ACK, duplicate ACK,
go-back on timeout, give up after 10, receiver linger and idle) is checked
exactly and repeatably. The final test runs whole transfers between the real
sender and receiver through an in-memory channel that drops, corrupts and
duplicates 20% of the datagrams each way using a seeded xorshift generator and
a fake clock, over eight seeds, window sizes 1, 8 and 16, an empty file and a
file that is an exact multiple of 1024 bytes, and compares the bytes delivered
with the bytes sent. Layers 1 and 2 have 100% line and branch coverage. The
same code then runs unchanged against the real relay, driven by a real socket
and clock.

## Results

Pending: the four Task 6 configurations are being measured.

## Known Bugs or Issues

None known. Files larger than 16 MiB are refused, as the spec allows.

## Experience

The hardest part was deciding where the line between layer 2 and layer 3
falls. Once the state machines took the time as a parameter and returned
"send these, deliver this, wake me at" instead of acting, the I/O loop became
almost trivial and the lossy end-to-end unit test became possible, and that
test found more than running against the relay did. Small details matter: the
FIN is held back until every DATA packet is acknowledged, an ACK for a packet
never sent is ignored rather than trusted, and the receiver must keep
answering repeated FINs during its linger or a lost final ACK turns a
successful transfer into a sender that gives up.
