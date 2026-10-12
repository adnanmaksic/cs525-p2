# Submission Report

- Submission generated at 10/12/2026 at 05:49:07

- Machine info: Linux runnervmmprz5 6.17.0-1022-azure #22-Ubuntu SMP Mon Jul 27 17:24:03 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux

## Note to Students

Please read this report carefully before submission.
Ensure that all sections are complete and accurate.
Look for any errors in the build or test outputs.
If you find any issues, correct them before submitting.
Post any questions on the class discussion board for help.


---

## README

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

---


## Build Output

This section was generated by running `make all` in the project root directory.

```bash
make[1]: Entering directory '/home/runner/work/cs525-p2/cs525-p2'
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug/lab.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug/main.c.o
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address build/debug/lab.c.o build/debug/main.c.o -o build/debug/myapp_d -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs525-p2/cs525-p2'
make[1]: Entering directory '/home/runner/work/cs525-p2/cs525-p2'
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/lab.c -o build/release/lab.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/main.c -o build/release/main.c.o
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion build/release/lab.c.o build/release/main.c.o -o build/release/myapp 
make[1]: Leaving directory '/home/runner/work/cs525-p2/cs525-p2'
make[1]: Entering directory '/home/runner/work/cs525-p2/cs525-p2'
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/lab.c -o build/tests/lab.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/main.c -o build/tests/main.c.o
mkdir -p build/tests/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/lab-test.c -o build/tests/lab-test.c.o
mkdir -p build/tests/harness/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/harness/unity.c -o build/tests/harness/unity.c.o
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage build/tests/lab.c.o build/tests/main.c.o build/tests/lab-test.c.o build/tests/harness/unity.c.o -o build/tests/myapp_t -fprofile-arcs -ftest-coverage
make[1]: Leaving directory '/home/runner/work/cs525-p2/cs525-p2'
make[1]: Entering directory '/home/runner/work/cs525-p2/cs525-p2'
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/lab.c -o build/debug-test/lab.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug-test/main.c.o
mkdir -p build/debug-test/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/lab-test.c -o build/debug-test/lab-test.c.o
mkdir -p build/debug-test/harness/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -c tests/harness/unity.c -o build/debug-test/harness/unity.c.o
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address build/debug-test/lab.c.o build/debug-test/main.c.o build/debug-test/lab-test.c.o build/debug-test/harness/unity.c.o -o build/debug-test/myapp_td -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs525-p2/cs525-p2'
Builds completed. You can run the application with: ./build/release/myapp
You can run the debug build with: ./build/debug/myapp_d
You can run the test build with: ./build/tests/myapp_t
You can run the debug-test build with: ./build/debug-test/myapp_td
```

---

## Coverage Report

This section was generated by running `make report` in the project root directory.

```bash
tests/lab-test.c:737:test_checksum_rfc1071_example:PASS
tests/lab-test.c:738:test_checksum_odd_length:PASS
tests/lab-test.c:739:test_checksum_empty_and_carry:PASS
tests/lab-test.c:740:test_checksum_catches_every_single_bit_flip:PASS
tests/lab-test.c:741:test_encode_hi_example:PASS
tests/lab-test.c:742:test_encode_ack_example:PASS
tests/lab-test.c:743:test_encode_rejects_bad_input:PASS
tests/lab-test.c:744:test_encode_decode_round_trip_largest:PASS
tests/lab-test.c:745:test_decode_null_and_too_short:PASS
tests/lab-test.c:746:test_decode_length_mismatch:PASS
tests/lab-test.c:747:test_decode_unknown_type_and_reserved:PASS
tests/lab-test.c:748:test_sender_init_rejects_bad_arguments:PASS
tests/lab-test.c:749:test_sender_empty_file_is_one_fin:PASS
tests/lab-test.c:750:test_sender_example_2500_bytes:PASS
tests/lab-test.c:751:test_sender_exact_multiple_of_1024:PASS
tests/lab-test.c:752:test_sender_window_full:PASS
tests/lab-test.c:753:test_sender_cumulative_ack_slides_several:PASS
tests/lab-test.c:754:test_sender_duplicate_and_bogus_acks_ignored:PASS
tests/lab-test.c:755:test_sender_timeout_resends_whole_window:PASS
tests/lab-test.c:756:test_sender_gives_up_after_10_timeouts:PASS
tests/lab-test.c:757:test_sender_stop_and_wait:PASS
tests/lab-test.c:758:test_receiver_in_order_duplicate_and_gap:PASS
tests/lab-test.c:759:test_receiver_ignores_acks:PASS
tests/lab-test.c:760:test_receiver_fin_linger_and_repeated_fin:PASS
tests/lab-test.c:761:test_receiver_empty_file:PASS
tests/lab-test.c:762:test_receiver_gives_up_when_idle:PASS
tests/lab-test.c:763:test_transfer_clean_channel:PASS
tests/lab-test.c:764:test_transfer_lossy_channel_seeded:PASS

-----------------------
28 Tests 0 Failures 0 Ignored 
OK
./build/tests/myapp_t
tests/lab-test.c:737:test_checksum_rfc1071_example:PASS
tests/lab-test.c:738:test_checksum_odd_length:PASS
tests/lab-test.c:739:test_checksum_empty_and_carry:PASS
tests/lab-test.c:740:test_checksum_catches_every_single_bit_flip:PASS
tests/lab-test.c:741:test_encode_hi_example:PASS
tests/lab-test.c:742:test_encode_ack_example:PASS
tests/lab-test.c:743:test_encode_rejects_bad_input:PASS
tests/lab-test.c:744:test_encode_decode_round_trip_largest:PASS
tests/lab-test.c:745:test_decode_null_and_too_short:PASS
tests/lab-test.c:746:test_decode_length_mismatch:PASS
tests/lab-test.c:747:test_decode_unknown_type_and_reserved:PASS
tests/lab-test.c:748:test_sender_init_rejects_bad_arguments:PASS
tests/lab-test.c:749:test_sender_empty_file_is_one_fin:PASS
tests/lab-test.c:750:test_sender_example_2500_bytes:PASS
tests/lab-test.c:751:test_sender_exact_multiple_of_1024:PASS
tests/lab-test.c:752:test_sender_window_full:PASS
tests/lab-test.c:753:test_sender_cumulative_ack_slides_several:PASS
tests/lab-test.c:754:test_sender_duplicate_and_bogus_acks_ignored:PASS
tests/lab-test.c:755:test_sender_timeout_resends_whole_window:PASS
tests/lab-test.c:756:test_sender_gives_up_after_10_timeouts:PASS
tests/lab-test.c:757:test_sender_stop_and_wait:PASS
tests/lab-test.c:758:test_receiver_in_order_duplicate_and_gap:PASS
tests/lab-test.c:759:test_receiver_ignores_acks:PASS
tests/lab-test.c:760:test_receiver_fin_linger_and_repeated_fin:PASS
tests/lab-test.c:761:test_receiver_empty_file:PASS
tests/lab-test.c:762:test_receiver_gives_up_when_idle:PASS
tests/lab-test.c:763:test_transfer_clean_channel:PASS
tests/lab-test.c:764:test_transfer_lossy_channel_seeded:PASS

-----------------------
28 Tests 0 Failures 0 Ignored 
OK
mkdir -p ./build/report/html
mkdir -p ./build/report/txt
gcovr -r . --html --html-details --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$' -o ./build/report/html/coverage_report.html
(INFO) Reading coverage data...

(INFO) Writing coverage report...

gcovr -r . --txt                 --exclude-directories build/tests/harness --exclude '.*main\.c$' --exclude '.*test\.c$'
(INFO) Reading coverage data...

(INFO) Writing coverage report...

------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: .
------------------------------------------------------------------------------
File                                       Lines     Exec  Cover   Missing
------------------------------------------------------------------------------
src/lab.c                                    168      168   100%
------------------------------------------------------------------------------
TOTAL                                        168      168   100%
------------------------------------------------------------------------------
```

---

## Address Sanitizer Report

This section was generated by running `make leak-test` in the project root directory.

```bash
tests/lab-test.c:737:test_checksum_rfc1071_example:PASS
tests/lab-test.c:738:test_checksum_odd_length:PASS
tests/lab-test.c:739:test_checksum_empty_and_carry:PASS
tests/lab-test.c:740:test_checksum_catches_every_single_bit_flip:PASS
tests/lab-test.c:741:test_encode_hi_example:PASS
tests/lab-test.c:742:test_encode_ack_example:PASS
tests/lab-test.c:743:test_encode_rejects_bad_input:PASS
tests/lab-test.c:744:test_encode_decode_round_trip_largest:PASS
tests/lab-test.c:745:test_decode_null_and_too_short:PASS
tests/lab-test.c:746:test_decode_length_mismatch:PASS
tests/lab-test.c:747:test_decode_unknown_type_and_reserved:PASS
tests/lab-test.c:748:test_sender_init_rejects_bad_arguments:PASS
tests/lab-test.c:749:test_sender_empty_file_is_one_fin:PASS
tests/lab-test.c:750:test_sender_example_2500_bytes:PASS
tests/lab-test.c:751:test_sender_exact_multiple_of_1024:PASS
tests/lab-test.c:752:test_sender_window_full:PASS
tests/lab-test.c:753:test_sender_cumulative_ack_slides_several:PASS
tests/lab-test.c:754:test_sender_duplicate_and_bogus_acks_ignored:PASS
tests/lab-test.c:755:test_sender_timeout_resends_whole_window:PASS
tests/lab-test.c:756:test_sender_gives_up_after_10_timeouts:PASS
tests/lab-test.c:757:test_sender_stop_and_wait:PASS
tests/lab-test.c:758:test_receiver_in_order_duplicate_and_gap:PASS
tests/lab-test.c:759:test_receiver_ignores_acks:PASS
tests/lab-test.c:760:test_receiver_fin_linger_and_repeated_fin:PASS
tests/lab-test.c:761:test_receiver_empty_file:PASS
tests/lab-test.c:762:test_receiver_gives_up_when_idle:PASS
tests/lab-test.c:763:test_transfer_clean_channel:PASS
tests/lab-test.c:764:test_transfer_lossy_channel_seeded:PASS

-----------------------
28 Tests 0 Failures 0 Ignored 
OK
```

---

## Src Files
### lab.c

```c

#include "lab.h"
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------ */
/* Layer 1: packets                                                          */
/* ------------------------------------------------------------------------ */

uint16_t inet_checksum(const uint8_t *buf, size_t len)
{
  uint32_t sum = 0;
  size_t i = 0;

  for (; i + 1 < len; i += 2)
  {
    sum += (uint32_t)((buf[i] << 8) | buf[i + 1]);
  }
  if (i < len)
  {
    sum += (uint32_t)(buf[i] << 8); // odd length: pad with a zero byte
  }
  while (sum >> 16)
  {
    sum = (sum & 0xffffu) + (sum >> 16);
  }
  return (uint16_t)~sum;
}

size_t pkt_encode(const struct packet *pkt, uint8_t *buf, size_t cap)
{
  if (pkt == NULL || buf == NULL || pkt->len > PKT_MAX_PAYLOAD ||
      pkt->type > PKT_FIN)
  {
    return 0;
  }
  size_t total = PKT_HDR_LEN + pkt->len;
  if (cap < total)
  {
    return 0;
  }

  uint16_t zero = 0;
  uint32_t seq = htonl(pkt->seq);
  uint16_t len = htons(pkt->len);
  buf[0] = pkt->type;
  buf[1] = 0;
  memcpy(buf + 2, &zero, sizeof zero);
  memcpy(buf + 4, &seq, sizeof seq);
  memcpy(buf + 8, &len, sizeof len);
  memcpy(buf + PKT_HDR_LEN, pkt->payload, pkt->len);

  uint16_t sum = htons(inet_checksum(buf, total));
  memcpy(buf + 2, &sum, sizeof sum);
  return total;
}

bool pkt_decode(const uint8_t *buf, size_t len, struct packet *out)
{
  if (buf == NULL || out == NULL || len < PKT_HDR_LEN)
  {
    return false;
  }

  uint16_t plen;
  memcpy(&plen, buf + 8, sizeof plen);
  plen = ntohs(plen);
  if (plen > PKT_MAX_PAYLOAD || PKT_HDR_LEN + plen != len)
  {
    return false;
  }
  if (buf[0] > PKT_FIN || buf[1] != 0)
  {
    return false;
  }
  if (inet_checksum(buf, len) != 0)
  {
    return false;
  }

  uint32_t seq;
  memcpy(&seq, buf + 4, sizeof seq);
  out->type = buf[0];
  out->seq = ntohl(seq);
  out->len = plen;
  memcpy(out->payload, buf + PKT_HDR_LEN, plen);
  return true;
}

/* ------------------------------------------------------------------------ */
/* Layer 2: Go-Back-N sender                                                 */
/* ------------------------------------------------------------------------ */

int gbn_sender_init(struct gbn_sender *s, const uint8_t *data, size_t len,
                    uint32_t window, uint64_t timeout_ms)
{
  if (s == NULL || (data == NULL && len > 0) || len > GBN_MAX_FILE ||
      window < GBN_MIN_WINDOW || window > GBN_MAX_WINDOW || timeout_ms == 0)
  {
    return -1;
  }

  memset(s, 0, sizeof *s);
  s->slots = malloc((size_t)window * PKT_MAX_LEN);
  s->slot_len = calloc(window, sizeof *s->slot_len);
  if (s->slots == NULL || s->slot_len == NULL) // GCOVR_EXCL_START
  {
    gbn_sender_destroy(s);
    return -1;
  } // GCOVR_EXCL_STOP

  s->data = data;
  s->data_len = len;
  s->ndata = (uint32_t)((len + PKT_MAX_PAYLOAD - 1) / PKT_MAX_PAYLOAD);
  s->window = window;
  s->timeout_ms = timeout_ms;
  s->status = GBN_RUNNING;
  return 0;
}

void gbn_sender_destroy(struct gbn_sender *s)
{
  if (s == NULL)
  {
    return;
  }
  free(s->slots);
  free(s->slot_len);
  s->slots = NULL;
  s->slot_len = NULL;
}

/* Encodes packet seq (DATA, or the FIN when seq == ndata) into its slot. */
static void sender_build(struct gbn_sender *s, uint32_t seq)
{
  struct packet pkt;
  pkt.seq = seq;
  if (seq < s->ndata)
  {
    size_t off = (size_t)seq * PKT_MAX_PAYLOAD;
    size_t n = s->data_len - off;
    if (n > PKT_MAX_PAYLOAD)
    {
      n = PKT_MAX_PAYLOAD;
    }
    pkt.type = PKT_DATA;
    pkt.len = (uint16_t)n;
    memcpy(pkt.payload, s->data + off, n);
  }
  else
  {
    pkt.type = PKT_FIN;
    pkt.len = 0;
  }
  size_t slot = seq % s->window;
  s->slot_len[slot] = pkt_encode(&pkt, s->slots + slot * PKT_MAX_LEN, PKT_MAX_LEN);
}

static void tx_add(const struct gbn_sender *s, uint32_t seq, struct gbn_tx *tx)
{
  size_t slot = seq % s->window;
  tx->pkt[tx->count] = s->slots + slot * PKT_MAX_LEN;
  tx->len[tx->count] = s->slot_len[slot];
  tx->count++;
}

size_t gbn_sender_pump(struct gbn_sender *s, uint64_t now, struct gbn_tx *tx)
{
  tx->count = 0;
  if (s->status != GBN_RUNNING)
  {
    return 0;
  }

  while (s->next < s->ndata && s->next < s->base + s->window)
  {
    sender_build(s, s->next);
    tx_add(s, s->next, tx);
    s->next++;
  }
  // The FIN goes out only once every DATA packet is acknowledged.
  if (s->next == s->ndata && s->base == s->ndata)
  {
    sender_build(s, s->next);
    tx_add(s, s->next, tx);
    s->next++;
  }

  if (tx->count > 0 && !s->timer_running)
  {
    s->timer_running = true;
    s->deadline = now + s->timeout_ms;
  }
  return tx->count;
}

size_t gbn_sender_on_packet(struct gbn_sender *s, const struct packet *pkt,
                            uint64_t now, struct gbn_tx *tx)
{
  tx->count = 0;
  // Duplicate ACKs (seq <= base) and ACKs for packets never sent change nothing.
  if (s->status != GBN_RUNNING || pkt->type != PKT_ACK || pkt->seq <= s->base ||
      pkt->seq > s->next)
  {
    return 0;
  }

  s->base = pkt->seq;
  s->timeouts = 0;
  if (s->base == s->ndata + 1)
  {
    s->status = GBN_DONE; // the FIN is acknowledged
    s->timer_running = false;
    return 0;
  }
  if (s->base < s->next)
  {
    s->deadline = now + s->timeout_ms;
  }
  else
  {
    s->timer_running = false;
  }
  return gbn_sender_pump(s, now, tx);
}

size_t gbn_sender_on_tick(struct gbn_sender *s, uint64_t now, struct gbn_tx *tx)
{
  tx->count = 0;
  if (s->status != GBN_RUNNING || !s->timer_running || now < s->deadline)
  {
    return 0;
  }

  s->timeouts++;
  if (s->timeouts >= GBN_MAX_TIMEOUTS)
  {
    s->status = GBN_FAILED;
    s->timer_running = false;
    return 0;
  }

  // Go back: resend everything in flight.
  for (uint32_t seq = s->base; seq < s->next; seq++)
  {
    tx_add(s, seq, tx);
  }
  s->deadline = now + s->timeout_ms;
  return tx->count;
}

int64_t gbn_sender_wait_ms(const struct gbn_sender *s, uint64_t now)
{
  if (!s->timer_running)
  {
    return -1;
  }
  return now >= s->deadline ? 0 : (int64_t)(s->deadline - now);
}

/* ------------------------------------------------------------------------ */
/* Layer 2: Go-Back-N receiver                                               */
/* ------------------------------------------------------------------------ */

void gbn_receiver_init(struct gbn_receiver *r, uint64_t now)
{
  memset(r, 0, sizeof *r);
  r->idle_until = now + GBN_IDLE_MS;
  r->status = GBN_RUNNING;
}

void gbn_receiver_on_packet(struct gbn_receiver *r, const struct packet *pkt,
                            uint64_t now, struct gbn_rx *out)
{
  memset(out, 0, sizeof *out);
  if (r->status != GBN_RUNNING || pkt->type == PKT_ACK)
  {
    return;
  }
  r->idle_until = now + GBN_IDLE_MS;

  if (!r->fin_seen && pkt->seq == r->expected)
  {
    if (pkt->type == PKT_DATA)
    {
      out->data = pkt->payload;
      out->data_len = pkt->len;
    }
    else
    {
      r->fin_seen = true;
      r->linger_until = now + GBN_LINGER_MS;
      out->finished = true;
    }
    r->expected++;
  }
  // In order or not, every DATA and FIN gets ACK expected.

  struct packet ack = {.type = PKT_ACK, .seq = r->expected, .len = 0};
  out->ack_len = pkt_encode(&ack, out->ack, sizeof out->ack);
}

enum gbn_status gbn_receiver_on_tick(struct gbn_receiver *r, uint64_t now)
{
  if (r->status == GBN_RUNNING)
  {
    if (r->fin_seen && now >= r->linger_until)
    {
      r->status = GBN_DONE;
    }
    else if (!r->fin_seen && now >= r->idle_until)
    {
      r->status = GBN_FAILED;
    }
  }
  return r->status;
}

int64_t gbn_receiver_wait_ms(const struct gbn_receiver *r, uint64_t now)
{
  uint64_t deadline = r->fin_seen ? r->linger_until : r->idle_until;
  if (r->status != GBN_RUNNING || now >= deadline)
  {
    return 0;
  }
  return (int64_t)(deadline - now);
}

```

### lab.h

```c

#ifndef LAB_H
#define LAB_H

/*
 * Go-Back-N reliable transfer over UDP.
 *
 * This header holds the two layers of the program that never touch a socket,
 * a clock or a file, so they can be unit tested with a fake clock and an
 * in-memory channel:
 *
 *   Layer 1, packets:        checksum, encode, decode and validate.
 *   Layer 2, state machines: the Go-Back-N sender and receiver. Time is always
 *                            passed in, in milliseconds, and every function
 *                            reports what to send, what to deliver and when
 *                            the timer is due.
 *
 * Layer 3, the I/O (socket, relay hello, poll loop, clock and files), lives in
 * main.c and only drives the functions declared here.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ------------------------------------------------------------------------ */
/* Layer 1: packets                                                          */
/* ------------------------------------------------------------------------ */

#define PKT_HDR_LEN 10u
#define PKT_MAX_PAYLOAD 1024u
#define PKT_MAX_LEN (PKT_HDR_LEN + PKT_MAX_PAYLOAD)

/** Packet types as they appear in the first byte on the wire. */
enum pkt_type
{
  PKT_DATA = 0,
  PKT_ACK = 1,
  PKT_FIN = 2
};

/** A decoded packet. Only the first len bytes of payload are meaningful. */
struct packet
{
  uint8_t type;
  uint32_t seq;
  uint16_t len;
  uint8_t payload[PKT_MAX_PAYLOAD];
};

/**
 * @brief Computes the RFC 1071 Internet checksum of a buffer.
 *
 * The buffer is summed as big-endian 16-bit words in one's complement
 * arithmetic, with an odd final byte padded by a zero byte, and the one's
 * complement of the sum is returned. Run over a packet whose checksum field
 * is filled in, an undamaged packet yields 0.
 *
 * @param buf The bytes to sum, may be NULL only when len is 0.
 * @param len The number of bytes.
 * @return The checksum in host byte order.
 */
uint16_t inet_checksum(const uint8_t *buf, size_t len);

/**
 * @brief Encodes a packet into its wire format, checksum included.
 *
 * @param pkt The packet to encode. Its len must be at most PKT_MAX_PAYLOAD.
 * @param buf Where to write the datagram.
 * @param cap The size of buf in bytes.
 * @return The number of bytes written, or 0 if pkt is invalid or buf is too
 *         small.
 */
size_t pkt_encode(const struct packet *pkt, uint8_t *buf, size_t cap);

/**
 * @brief Decodes and validates one received datagram.
 *
 * The datagram is rejected unless it is at least PKT_HDR_LEN bytes, its length
 * field matches its size and is at most PKT_MAX_PAYLOAD, its type is known,
 * its reserved byte is 0 and its checksum verifies.
 *
 * @param buf The datagram as received.
 * @param len The size recvfrom returned.
 * @param out Where to store the packet; untouched on failure.
 * @return true if the datagram is a valid packet.
 */
bool pkt_decode(const uint8_t *buf, size_t len, struct packet *out);

/* ------------------------------------------------------------------------ */
/* Layer 2: Go-Back-N state machines                                         */
/* ------------------------------------------------------------------------ */

#define GBN_MIN_WINDOW 1u
#define GBN_MAX_WINDOW 64u
#define GBN_MAX_TIMEOUTS 10u
#define GBN_MAX_FILE (16u * 1024u * 1024u)
#define GBN_LINGER_MS 2000u
#define GBN_IDLE_MS 30000u

/** Where a sender or receiver is in its life. */
enum gbn_status
{
  GBN_RUNNING = 0,
  GBN_DONE = 1,
  GBN_FAILED = 2
};

/** The packets a sender wants put on the wire, in order. */
struct gbn_tx
{
  size_t count;
  const uint8_t *pkt[GBN_MAX_WINDOW];
  size_t len[GBN_MAX_WINDOW];
};

/** Go-Back-N sender. Treat the fields as read-only outside lab.c. */
struct gbn_sender
{
  const uint8_t *data;   /* the whole file, owned by the caller */
  size_t data_len;
  uint32_t ndata;        /* DATA packets; the FIN is seq ndata */
  uint32_t window;
  uint64_t timeout_ms;
  uint32_t base;         /* oldest unacknowledged packet */
  uint32_t next;         /* next packet never sent */
  uint8_t *slots;        /* window copies of in-flight packets, by seq % window */
  size_t *slot_len;
  bool timer_running;
  uint64_t deadline;     /* when the timer runs out, if running */
  uint32_t timeouts;     /* timeouts in a row without progress */
  enum gbn_status status;
};

/**
 * @brief Prepares a sender for a file held in memory.
 *
 * @param s The sender to initialize.
 * @param data The file contents; must outlive the sender. May be NULL if len
 *             is 0.
 * @param len The file size, at most GBN_MAX_FILE.
 * @param window The window size, GBN_MIN_WINDOW to GBN_MAX_WINDOW.
 * @param timeout_ms The retransmission timeout, greater than 0.
 * @return 0 on success, -1 on invalid arguments or allocation failure.
 */
int gbn_sender_init(struct gbn_sender *s, const uint8_t *data, size_t len,
                    uint32_t window, uint64_t timeout_ms);

/**
 * @brief Frees the retransmission buffers of a sender. Safe to call twice.
 * @param s The sender.
 */
void gbn_sender_destroy(struct gbn_sender *s);

/**
 * @brief Sends whatever the window allows: new DATA packets, or the FIN once
 *        every DATA packet is acknowledged. Starts the timer if it was idle.
 *
 * @param s The sender.
 * @param now The current time in milliseconds.
 * @param tx Filled with the packets to send.
 * @return The number of packets in tx.
 */
size_t gbn_sender_pump(struct gbn_sender *s, uint64_t now, struct gbn_tx *tx);

/**
 * @brief Feeds a received packet to the sender.
 *
 * An ACK whose seq is beyond base slides the window, restarts or stops the
 * timer and then sends what the wider window allows. Anything else, including
 * a duplicate ACK, changes nothing.
 *
 * @param s The sender.
 * @param pkt A packet that passed pkt_decode.
 * @param now The current time in milliseconds.
 * @param tx Filled with the packets to send.
 * @return The number of packets in tx.
 */
size_t gbn_sender_on_packet(struct gbn_sender *s, const struct packet *pkt,
                            uint64_t now, struct gbn_tx *tx);

/**
 * @brief Tells the sender the time, so it can act on an expired timer.
 *
 * If the timer has run out, every packet from base to next - 1 is resent and
 * the timer restarted, unless this is the GBN_MAX_TIMEOUTS-th timeout in a row
 * without progress, in which case the sender fails.
 *
 * @param s The sender.
 * @param now The current time in milliseconds.
 * @param tx Filled with the packets to resend.
 * @return The number of packets in tx.
 */
size_t gbn_sender_on_tick(struct gbn_sender *s, uint64_t now, struct gbn_tx *tx);

/**
 * @brief How long the I/O layer may wait before calling gbn_sender_on_tick.
 * @param s The sender.
 * @param now The current time in milliseconds.
 * @return Milliseconds until the timer is due, 0 if it is already due, or -1
 *         if no timer is running.
 */
int64_t gbn_sender_wait_ms(const struct gbn_sender *s, uint64_t now);

/** What the receiver wants done with one packet. */
struct gbn_rx
{
  uint8_t ack[PKT_HDR_LEN]; /* encoded ACK to send, if ack_len > 0 */
  size_t ack_len;
  const uint8_t *data;      /* payload to append to the file, if data_len > 0 */
  size_t data_len;
  bool finished;            /* the FIN was just accepted: close the file */
};

/** Go-Back-N receiver. Treat the fields as read-only outside lab.c. */
struct gbn_receiver
{
  uint32_t expected;     /* next packet wanted */
  bool fin_seen;
  uint64_t linger_until; /* valid once fin_seen */
  uint64_t idle_until;   /* give up if nothing valid arrives before this */
  enum gbn_status status;
};

/**
 * @brief Prepares a receiver that starts waiting at time now.
 * @param r The receiver.
 * @param now The current time in milliseconds.
 */
void gbn_receiver_init(struct gbn_receiver *r, uint64_t now);

/**
 * @brief Feeds a received packet to the receiver.
 *
 * DATA with seq == expected is delivered and acknowledged; a FIN with seq ==
 * expected finishes the file and starts the linger; anything else DATA or FIN
 * is discarded and re-acknowledged with the same ACK. ACKs are ignored.
 *
 * @param r The receiver.
 * @param pkt A packet that passed pkt_decode. out->data points into it.
 * @param now The current time in milliseconds.
 * @param out Filled with what to send, deliver and close.
 */
void gbn_receiver_on_packet(struct gbn_receiver *r, const struct packet *pkt,
                            uint64_t now, struct gbn_rx *out);

/**
 * @brief Tells the receiver the time, ending the linger or giving up when idle.
 * @param r The receiver.
 * @param now The current time in milliseconds.
 * @return The receiver's status after the tick.
 */
enum gbn_status gbn_receiver_on_tick(struct gbn_receiver *r, uint64_t now);

/**
 * @brief How long the I/O layer may wait before calling gbn_receiver_on_tick.
 * @param r The receiver.
 * @param now The current time in milliseconds.
 * @return Milliseconds until the next deadline, 0 if it has passed.
 */
int64_t gbn_receiver_wait_ms(const struct gbn_receiver *r, uint64_t now);

#endif // LAB_H

```

### main.c

```c

/*
 * Layer 3: the I/O. The socket, the relay hello, the poll loop, the clock and
 * the files live here and nowhere else. Each loop reads one event, hands it to
 * a state machine from lab.h, and carries out what comes back.
 */
#define _POSIX_C_SOURCE 200809L
#include "lab.h"
#include <errno.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef TEST
#define main main_exclude
#endif

#define EXIT_USAGE 1
#define EXIT_NET 2
#define HELLO_TRIES 5
#define HELLO_WAIT_MS 1000

static const char *usage_text =
    "Usage: myapp send -s <session> [-w window] [-T timeout-ms] [-l loss]\n"
    "                  [-c corrupt] [-d dup] [-p port] <relay> <file>\n"
    "       myapp recv -s <session> [-p port] <relay> <file>\n"
    "\n"
    "  -s <session>     session name shared by the sender and the receiver\n"
    "  -w <window>      Go-Back-N window size in packets, 1 to 64 (default: 8)\n"
    "  -T <timeout-ms>  retransmission timeout in milliseconds (default: 250)\n"
    "  -l <loss>        probability the relay drops a packet (default: 0)\n"
    "  -c <corrupt>     probability the relay flips a bit (default: 0)\n"
    "  -d <dup>         probability the relay duplicates a packet (default: 0)\n"
    "  -p <port>        relay port (default: 4250)\n"
    "  <relay>          host name or address of the relay\n"
    "  <file>           file to send, or file to write what is received\n";

struct options
{
  bool sender;
  const char *session;
  long window;
  long timeout_ms;
  double loss;
  double corrupt;
  double dup;
  const char *port;
  const char *relay;
  const char *file;
};

static uint64_t now_ms(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000u + (uint64_t)ts.tv_nsec / 1000000u;
}

static int poll_ms(int64_t ms)
{
  if (ms < 0)
  {
    return -1;
  }
  return ms > 60000 ? 60000 : (int)ms;
}

/* ---------------------------- command line ------------------------------ */

static bool parse_long(const char *s, long lo, long hi, long *out)
{
  char *end;
  errno = 0;
  long v = strtol(s, &end, 10);
  if (errno != 0 || end == s || *end != '\0' || v < lo || v > hi)
  {
    return false;
  }
  *out = v;
  return true;
}

static bool parse_prob(const char *s, double *out)
{
  char *end;
  errno = 0;
  double v = strtod(s, &end);
  if (errno != 0 || end == s || *end != '\0' || !(v >= 0.0 && v <= 0.5))
  {
    return false;
  }
  *out = v;
  return true;
}

static bool valid_session(const char *s)
{
  size_t n = strlen(s);
  if (n < 1 || n > 32)
  {
    return false;
  }
  for (size_t i = 0; i < n; i++)
  {
    char c = s[i];
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
    {
      return false;
    }
  }
  return true;
}

static int bad_usage(const char *why)
{
  fprintf(stderr, "myapp: %s\n\n%s", why, usage_text);
  return EXIT_USAGE;
}

/* Returns 0 if the options are good, otherwise the exit code. */
static int parse_args(int argc, char **argv, struct options *o)
{
  memset(o, 0, sizeof *o);
  o->window = 8;
  o->timeout_ms = 250;
  o->port = "4250";

  if (strcmp(argv[1], "send") == 0)
  {
    o->sender = true;
  }
  else if (strcmp(argv[1], "recv") != 0)
  {
    return bad_usage("the mode must be send or recv");
  }

  const char *optstring = o->sender ? "s:w:T:l:c:d:p:" : "s:p:";
  long port;
  int c;
  opterr = 0;
  optind = 1;
  while ((c = getopt(argc - 1, argv + 1, optstring)) != -1)
  {
    switch (c)
    {
    case 's':
      if (!valid_session(optarg))
        return bad_usage("-s takes 1 to 32 characters from a-z, 0-9 and -");
      o->session = optarg;
      break;
    case 'w':
      if (!parse_long(optarg, GBN_MIN_WINDOW, GBN_MAX_WINDOW, &o->window))
        return bad_usage("-w takes a window from 1 to 64");
      break;
    case 'T':
      if (!parse_long(optarg, 1, 600000, &o->timeout_ms))
        return bad_usage("-T takes a timeout in milliseconds, at least 1");
      break;
    case 'l':
      if (!parse_prob(optarg, &o->loss))
        return bad_usage("-l takes a probability from 0 to 0.5");
      break;
    case 'c':
      if (!parse_prob(optarg, &o->corrupt))
        return bad_usage("-c takes a probability from 0 to 0.5");
      break;
    case 'd':
      if (!parse_prob(optarg, &o->dup))
        return bad_usage("-d takes a probability from 0 to 0.5");
      break;
    case 'p':
      if (!parse_long(optarg, 1, 65535, &port))
        return bad_usage("-p takes a port from 1 to 65535");
      o->port = optarg;
      break;
    default:
      return bad_usage("unknown option or missing argument");
    }
  }

  if (o->session == NULL)
  {
    return bad_usage("-s <session> is required");
  }
  if (argc - 1 - optind != 2)
  {
    return bad_usage("expected <relay> and <file>");
  }
  o->relay = argv[1 + optind];
  o->file = argv[2 + optind];
  return 0;
}

/* ------------------------------ socket ---------------------------------- */

/* Resolves the relay and opens one connected UDP socket for the whole run. */
static int open_socket(const char *host, const char *port)
{
  struct addrinfo hints;
  struct addrinfo *res = NULL;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_DGRAM;

  int rc = getaddrinfo(host, port, &hints, &res);
  if (rc != 0)
  {
    fprintf(stderr, "myapp: cannot resolve %s: %s\n", host, gai_strerror(rc));
    return -1;
  }

  // Try IPv4 addresses first: the relay listens on IPv4.
  int fd = -1;
  for (int pass = 0; pass < 2 && fd < 0; pass++)
  {
    for (struct addrinfo *ai = res; ai != NULL && fd < 0; ai = ai->ai_next)
    {
      if ((pass == 0) != (ai->ai_family == AF_INET))
      {
        continue;
      }
      fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
      if (fd >= 0 && connect(fd, ai->ai_addr, ai->ai_addrlen) != 0)
      {
        close(fd);
        fd = -1;
      }
    }
  }
  freeaddrinfo(res);
  if (fd < 0)
  {
    fprintf(stderr, "myapp: cannot open a socket to %s:%s\n", host, port);
  }
  return fd;
}

/* Sends one datagram; a refused or failed send is just a lost packet. */
static void send_dgram(int fd, const uint8_t *buf, size_t len)
{
  (void)send(fd, buf, len, 0);
}

static void send_all(int fd, const struct gbn_tx *tx)
{
  for (size_t i = 0; i < tx->count; i++)
  {
    send_dgram(fd, tx->pkt[i], tx->len[i]);
  }
}

/* Registers with the relay. Returns 0 on OK, otherwise prints why and fails. */
static int relay_hello(int fd, const char *hello)
{
  uint8_t buf[PKT_MAX_LEN + 1];

  for (int attempt = 0; attempt < HELLO_TRIES; attempt++)
  {
    send_dgram(fd, (const uint8_t *)hello, strlen(hello));
    uint64_t until = now_ms() + HELLO_WAIT_MS;
    uint64_t t;
    while ((t = now_ms()) < until)
    {
      struct pollfd pfd = {.fd = fd, .events = POLLIN};
      int n = poll(&pfd, 1, poll_ms((int64_t)(until - t)));
      if (n <= 0)
      {
        continue;
      }
      ssize_t got = recv(fd, buf, sizeof buf - 1, 0);
      if (got < 0)
      {
        continue; // e.g. ECONNREFUSED: nobody is listening yet
      }
      buf[got] = '\0';
      if (strcmp((char *)buf, "OK") == 0)
      {
        return 0;
      }
      if (strncmp((char *)buf, "ERR", 3) == 0)
      {
        fprintf(stderr, "myapp: relay refused: %s\n", (char *)buf + (got > 3 ? 4 : 3));
        return -1;
      }
      // Anything else is not a reply to the hello: keep waiting.
    }
  }
  fprintf(stderr, "myapp: no reply from the relay after %d tries\n", HELLO_TRIES);
  return -1;
}

/* ------------------------------ sender ---------------------------------- */

static uint8_t *read_file(const char *path, size_t *len)
{
  FILE *f = fopen(path, "rb");
  if (f == NULL)
  {
    fprintf(stderr, "myapp: cannot open %s: %s\n", path, strerror(errno));
    return NULL;
  }
  size_t cap = 64 * 1024;
  size_t n = 0;
  uint8_t *data = malloc(cap);
  while (data != NULL)
  {
    n += fread(data + n, 1, cap - n, f);
    if (n < cap || cap > GBN_MAX_FILE)
    {
      break;
    }
    uint8_t *bigger = realloc(data, cap * 2);
    if (bigger == NULL)
    {
      free(data);
      data = NULL;
      break;
    }
    data = bigger;
    cap *= 2;
  }
  bool bad = data == NULL || ferror(f) || n > GBN_MAX_FILE;
  fclose(f);
  if (bad)
  {
    fprintf(stderr, "myapp: cannot read %s, or it is larger than 16 MiB\n", path);
    free(data);
    return NULL;
  }
  *len = n;
  return data;
}

static int run_sender(int fd, const struct options *o)
{
  size_t len = 0;
  uint8_t *data = read_file(o->file, &len);
  if (data == NULL)
  {
    return EXIT_NET;
  }

  struct gbn_sender s;
  if (gbn_sender_init(&s, data, len, (uint32_t)o->window, (uint64_t)o->timeout_ms) != 0)
  {
    fprintf(stderr, "myapp: cannot start the sender\n");
    free(data);
    return EXIT_NET;
  }

  struct gbn_tx tx;
  uint8_t buf[PKT_MAX_LEN + 1];
  struct packet pkt;
  gbn_sender_pump(&s, now_ms(), &tx);
  send_all(fd, &tx);

  while (s.status == GBN_RUNNING)
  {
    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    int n = poll(&pfd, 1, poll_ms(gbn_sender_wait_ms(&s, now_ms())));
    if (n < 0 && errno != EINTR)
    {
      perror("myapp: poll");
      break;
    }
    if (n > 0)
    {
      ssize_t got = recv(fd, buf, sizeof buf, 0);
      if (got >= 0 && pkt_decode(buf, (size_t)got, &pkt))
      {
        gbn_sender_on_packet(&s, &pkt, now_ms(), &tx);
        send_all(fd, &tx);
      }
    }
    gbn_sender_on_tick(&s, now_ms(), &tx);
    send_all(fd, &tx);
  }

  int rc = 0;
  if (s.status == GBN_DONE)
  {
    printf("sent %zu bytes in %u packets\n", len, s.ndata);
  }
  else
  {
    fprintf(stderr, "myapp: giving up after %u timeouts in a row\n", s.timeouts);
    rc = EXIT_NET;
  }
  gbn_sender_destroy(&s);
  free(data);
  return rc;
}

/* ------------------------------ receiver -------------------------------- */

static int run_receiver(int fd, const struct options *o)
{
  FILE *f = fopen(o->file, "wb");
  if (f == NULL)
  {
    fprintf(stderr, "myapp: cannot create %s: %s\n", o->file, strerror(errno));
    return EXIT_NET;
  }

  struct gbn_receiver r;
  struct gbn_rx rx;
  uint8_t buf[PKT_MAX_LEN + 1];
  struct packet pkt;
  size_t total = 0;
  bool write_failed = false;
  gbn_receiver_init(&r, now_ms());

  while (gbn_receiver_on_tick(&r, now_ms()) == GBN_RUNNING)
  {
    struct pollfd pfd = {.fd = fd, .events = POLLIN};
    int n = poll(&pfd, 1, poll_ms(gbn_receiver_wait_ms(&r, now_ms())));
    if (n < 0 && errno != EINTR)
    {
      perror("myapp: poll");
      break;
    }
    if (n <= 0)
    {
      continue;
    }
    ssize_t got = recv(fd, buf, sizeof buf, 0);
    if (got < 0 || !pkt_decode(buf, (size_t)got, &pkt))
    {
      continue; // damaged or malformed: treated exactly like a loss
    }
    gbn_receiver_on_packet(&r, &pkt, now_ms(), &rx);
    if (rx.data_len > 0 && fwrite(rx.data, 1, rx.data_len, f) != rx.data_len)
    {
      write_failed = true;
    }
    total += rx.data_len;
    if (rx.finished)
    {
      write_failed = fclose(f) != 0 || write_failed;
      f = NULL;
    }
    if (rx.ack_len > 0)
    {
      send_dgram(fd, rx.ack, rx.ack_len);
    }
  }

  if (f != NULL)
  {
    fclose(f);
  }
  if (write_failed)
  {
    fprintf(stderr, "myapp: writing %s failed\n", o->file);
    return EXIT_NET;
  }
  if (r.status != GBN_DONE)
  {
    fprintf(stderr, "myapp: nothing arrived for %u seconds, giving up\n",
            GBN_IDLE_MS / 1000u);
    return EXIT_NET;
  }
  printf("received %zu bytes in %u packets\n", total, r.expected - 1);
  return 0;
}

/* -------------------------------- main ---------------------------------- */

int main(int argc, char **argv)
{
  if (argc < 2)
  {
    fputs(usage_text, stdout);
    return 0;
  }

  struct options o;
  int rc = parse_args(argc, argv, &o);
  if (rc != 0)
  {
    return rc;
  }

  char hello[128];
  if (o.sender)
  {
    snprintf(hello, sizeof hello, "HELLO %s send %g %g %g", o.session, o.loss,
             o.corrupt, o.dup);
  }
  else
  {
    snprintf(hello, sizeof hello, "HELLO %s recv", o.session);
  }

  int fd = open_socket(o.relay, o.port);
  if (fd < 0)
  {
    return EXIT_NET;
  }
  if (relay_hello(fd, hello) != 0)
  {
    close(fd);
    return EXIT_NET;
  }

  rc = o.sender ? run_sender(fd, &o) : run_receiver(fd, &o);
  close(fd);
  return rc;
}

```

## Tests Files
### lab-test.c

```c

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "harness/unity.h"
#include "../src/lab.h"

void setUp(void) {
}

void tearDown(void) {
}

/* ------------------------------------------------------------------------ */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------ */

/* Decodes packet i of tx, failing the test if it does not decode. */
static struct packet tx_packet(const struct gbn_tx *tx, size_t i) {
  struct packet p;
  TEST_ASSERT_TRUE(i < tx->count);
  TEST_ASSERT_TRUE(pkt_decode(tx->pkt[i], tx->len[i], &p));
  return p;
}

static struct packet make_ack(uint32_t seq) {
  struct packet p;
  memset(&p, 0, sizeof p);
  p.type = PKT_ACK;
  p.seq = seq;
  return p;
}

static struct packet make_data(uint32_t seq, const char *text) {
  struct packet p;
  memset(&p, 0, sizeof p);
  p.type = PKT_DATA;
  p.seq = seq;
  p.len = (uint16_t)strlen(text);
  memcpy(p.payload, text, p.len);
  return p;
}

static struct packet make_fin(uint32_t seq) {
  struct packet p;
  memset(&p, 0, sizeof p);
  p.type = PKT_FIN;
  p.seq = seq;
  return p;
}

/* Checks that an encoded ACK in a receiver result carries seq. */
static void assert_ack(const struct gbn_rx *rx, uint32_t seq) {
  struct packet p;
  TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN, rx->ack_len);
  TEST_ASSERT_TRUE(pkt_decode(rx->ack, rx->ack_len, &p));
  TEST_ASSERT_EQUAL_UINT8(PKT_ACK, p.type);
  TEST_ASSERT_EQUAL_UINT32(seq, p.seq);
  TEST_ASSERT_EQUAL_UINT16(0, p.len);
}

/* Rewrites the checksum of a hand-edited datagram so only the edit is wrong. */
static void fix_checksum(uint8_t *buf, size_t len) {
  buf[2] = 0;
  buf[3] = 0;
  uint16_t sum = inet_checksum(buf, len);
  buf[2] = (uint8_t)(sum >> 8);
  buf[3] = (uint8_t)(sum & 0xff);
}

/* ------------------------------------------------------------------------ */
/* Layer 1: checksum                                                         */
/* ------------------------------------------------------------------------ */

void test_checksum_rfc1071_example(void) {
  const uint8_t bytes[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
  TEST_ASSERT_EQUAL_HEX16(0x220d, inet_checksum(bytes, sizeof bytes));
}

void test_checksum_odd_length(void) {
  // The "Hi!" packet with its checksum field zeroed: the sum is 0x696e.
  const uint8_t bytes[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                           0x02, 0x00, 0x03, 0x48, 0x69, 0x21};
  TEST_ASSERT_EQUAL_HEX16(0x9691, inet_checksum(bytes, sizeof bytes));
  const uint8_t one[] = {0x12};
  TEST_ASSERT_EQUAL_HEX16((uint16_t)~0x1200, inet_checksum(one, 1));
}

void test_checksum_empty_and_carry(void) {
  TEST_ASSERT_EQUAL_HEX16(0xffff, inet_checksum(NULL, 0));
  // ffff + ffff = 1fffe, folds to ffff, complement 0000.
  const uint8_t bytes[] = {0xff, 0xff, 0xff, 0xff};
  TEST_ASSERT_EQUAL_HEX16(0x0000, inet_checksum(bytes, sizeof bytes));
}

void test_checksum_catches_every_single_bit_flip(void) {
  uint8_t buf[PKT_MAX_LEN];
  struct packet p = make_data(2, "Hi!");
  struct packet out;
  size_t n = pkt_encode(&p, buf, sizeof buf);
  TEST_ASSERT_EQUAL_size_t(13, n);
  TEST_ASSERT_TRUE(pkt_decode(buf, n, &out));
  for (size_t bit = 0; bit < n * 8; bit++) {
    buf[bit / 8] ^= (uint8_t)(1u << (bit % 8));
    TEST_ASSERT_FALSE(pkt_decode(buf, n, &out));
    buf[bit / 8] ^= (uint8_t)(1u << (bit % 8));
  }
}

/* ------------------------------------------------------------------------ */
/* Layer 1: encode and decode                                                */
/* ------------------------------------------------------------------------ */

void test_encode_hi_example(void) {
  const uint8_t expect[] = {0x00, 0x00, 0x96, 0x91, 0x00, 0x00, 0x00,
                            0x02, 0x00, 0x03, 0x48, 0x69, 0x21};
  uint8_t buf[PKT_MAX_LEN];
  struct packet p = make_data(2, "Hi!");
  TEST_ASSERT_EQUAL_size_t(sizeof expect, pkt_encode(&p, buf, sizeof buf));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, buf, sizeof expect);
}

void test_encode_ack_example(void) {
  const uint8_t expect[] = {0x01, 0x00, 0xfe, 0xfc, 0x00,
                            0x00, 0x00, 0x03, 0x00, 0x00};
  uint8_t buf[PKT_HDR_LEN];
  struct packet p = make_ack(3);
  TEST_ASSERT_EQUAL_size_t(PKT_HDR_LEN, pkt_encode(&p, buf, sizeof buf));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expect, buf, sizeof expect);
}

void test_encode_rejects_bad_input(void) {
  uint8_t buf[PKT_MAX_LEN];
  struct packet p = make_data(0, "abc");
  TEST_ASSERT_EQUAL_size_t(0, pkt_encode(NULL, buf, sizeof buf));
  TEST_ASSERT_EQUAL_size_t(0, pkt_encode(&p, NULL, sizeof buf));
  TEST_ASSERT_EQUAL_size_t(0, pkt_encode(&p, buf, 12)); // needs 13
  p.len = PKT_MAX_PAYLOAD + 1;
  TEST_ASSERT_EQUAL_size_t(0, pkt_encode(&p, buf, sizeof buf));
  p.len = 3;
  p.type = 3;
  TEST_ASSERT_EQUAL_size_t(0, pkt_encode(&p, buf, sizeof buf));
}

void test_encode_decode_round_trip_largest(void) {
  uint8_t buf[PKT_MAX_LEN];
  struct packet p;
  struct packet out;
  memset(&p, 0, sizeof p);
  p.type = PKT_DATA;
  p.seq = 0x01020304;
  p.len = PKT_MAX_PAYLOAD;
  for (size_t i = 0; i < PKT_MAX_PAYLOAD; i++) {
    p.payload[i] = (uint8_t)(i * 7);
  }
  TEST_ASSERT_EQUAL_size_t(PKT_MAX_LEN, pkt_encode(&p, buf, sizeof buf));
  TEST_ASSERT_EQUAL_HEX8(0x01, buf[4]); // seq is big-endian on the wire
  TEST_ASSERT_EQUAL_HEX8(0x04, buf[7]);
  TEST_ASSERT_EQUAL_HEX8(0x04, buf[8]); // 1024 = 0x0400
  TEST_ASSERT_EQUAL_HEX8(0x00, buf[9]);
  TEST_ASSERT_TRUE(pkt_decode(buf, PKT_MAX_LEN, &out));
  TEST_ASSERT_EQUAL_UINT8(PKT_DATA, out.type);
  TEST_ASSERT_EQUAL_UINT32(0x01020304, out.seq);
  TEST_ASSERT_EQUAL_UINT16(PKT_MAX_PAYLOAD, out.len);
  TEST_ASSERT_EQUAL_UINT8_ARRAY(p.payload, out.payload, PKT_MAX_PAYLOAD);
}

void test_decode_null_and_too_short(void) {
  uint8_t buf[PKT_MAX_LEN];
  struct packet p = make_ack(1);
  struct packet out;
  size_t n = pkt_encode(&p, buf, sizeof buf);
  TEST_ASSERT_FALSE(pkt_decode(NULL, n, &out));
  TEST_ASSERT_FALSE(pkt_decode(buf, n, NULL));
  TEST_ASSERT_FALSE(pkt_decode(buf, 0, &out));
  TEST_ASSERT_FALSE(pkt_decode(buf, PKT_HDR_LEN - 1, &out));
  TEST_ASSERT_TRUE(pkt_decode(buf, n, &out));
}

void test_decode_length_mismatch(void) {
  uint8_t buf[PKT_MAX_LEN + 8];
  struct packet p = make_data(5, "hello");
  struct packet out;
  size_t n = pkt_encode(&p, buf, sizeof buf);
  TEST_ASSERT_FALSE(pkt_decode(buf, n - 1, &out)); // datagram shorter than length
  buf[n] = 0;
  TEST_ASSERT_FALSE(pkt_decode(buf, n + 1, &out)); // datagram longer than length

  // A length over 1024 is rejected even when it matches the datagram size.
  memset(buf, 0, sizeof buf);
  buf[8] = 0x04;
  buf[9] = 0x01; // 1025
  fix_checksum(buf, PKT_MAX_LEN + 1);
  TEST_ASSERT_FALSE(pkt_decode(buf, PKT_MAX_LEN + 1, &out));
}

void test_decode_unknown_type_and_reserved(void) {
  uint8_t buf[PKT_MAX_LEN];
  struct packet p = make_fin(4);
  struct packet out;
  size_t n = pkt_encode(&p, buf, sizeof buf);
  buf[0] = 3;
  fix_checksum(buf, n);
  TEST_ASSERT_FALSE(pkt_decode(buf, n, &out));
  buf[0] = 0xff;
  fix_checksum(buf, n);
  TEST_ASSERT_FALSE(pkt_decode(buf, n, &out));
  buf[0] = PKT_FIN;
  buf[1] = 1;
  fix_checksum(buf, n);
  TEST_ASSERT_FALSE(pkt_decode(buf, n, &out));
  buf[1] = 0;
  fix_checksum(buf, n);
  TEST_ASSERT_TRUE(pkt_decode(buf, n, &out));
  TEST_ASSERT_EQUAL_UINT8(PKT_FIN, out.type);
  TEST_ASSERT_EQUAL_UINT32(4, out.seq);
}

/* ------------------------------------------------------------------------ */
/* Layer 2: sender                                                           */
/* ------------------------------------------------------------------------ */

static uint8_t file10[10 * PKT_MAX_PAYLOAD];

static void fill(uint8_t *buf, size_t len, uint32_t seed) {
  for (size_t i = 0; i < len; i++) {
    seed = seed * 1103515245u + 12345u;
    buf[i] = (uint8_t)(seed >> 16);
  }
}

void test_sender_init_rejects_bad_arguments(void) {
  struct gbn_sender s;
  uint8_t d[4] = {0};
  TEST_ASSERT_EQUAL_INT(-1, gbn_sender_init(NULL, d, 4, 8, 250));
  TEST_ASSERT_EQUAL_INT(-1, gbn_sender_init(&s, NULL, 4, 8, 250));
  TEST_ASSERT_EQUAL_INT(-1, gbn_sender_init(&s, d, (size_t)GBN_MAX_FILE + 1, 8, 250));
  TEST_ASSERT_EQUAL_INT(-1, gbn_sender_init(&s, d, 4, 0, 250));
  TEST_ASSERT_EQUAL_INT(-1, gbn_sender_init(&s, d, 4, GBN_MAX_WINDOW + 1, 250));
  TEST_ASSERT_EQUAL_INT(-1, gbn_sender_init(&s, d, 4, 8, 0));
  TEST_ASSERT_EQUAL_INT(0, gbn_sender_init(&s, d, 4, GBN_MAX_WINDOW, 250));
  TEST_ASSERT_EQUAL_UINT32(1, s.ndata);
  // Nothing sent yet, so no timer is running and a tick does nothing.
  struct gbn_tx tx;
  TEST_ASSERT_EQUAL_INT64(-1, gbn_sender_wait_ms(&s, 0));
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_on_tick(&s, 100000, &tx));
  gbn_sender_destroy(&s);
  gbn_sender_destroy(&s); // twice is harmless
  gbn_sender_destroy(NULL);
}

void test_sender_empty_file_is_one_fin(void) {
  struct gbn_sender s;
  struct gbn_tx tx;
  TEST_ASSERT_EQUAL_INT(0, gbn_sender_init(&s, NULL, 0, 8, 250));
  TEST_ASSERT_EQUAL_size_t(1, gbn_sender_pump(&s, 0, &tx));
  struct packet p = tx_packet(&tx, 0);
  TEST_ASSERT_EQUAL_UINT8(PKT_FIN, p.type);
  TEST_ASSERT_EQUAL_UINT32(0, p.seq);
  TEST_ASSERT_EQUAL_UINT16(0, p.len);
  TEST_ASSERT_EQUAL_INT64(250, gbn_sender_wait_ms(&s, 0));

  struct packet ack = make_ack(1);
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_on_packet(&s, &ack, 10, &tx));
  TEST_ASSERT_EQUAL_INT(GBN_DONE, s.status);
  TEST_ASSERT_EQUAL_INT64(-1, gbn_sender_wait_ms(&s, 10));
  // Nothing more happens once done.
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_pump(&s, 20, &tx));
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_on_packet(&s, &ack, 20, &tx));
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_on_tick(&s, 1000, &tx));
  gbn_sender_destroy(&s);
}

void test_sender_example_2500_bytes(void) {
  struct gbn_sender s;
  struct gbn_tx tx;
  fill(file10, 2500, 1);
  TEST_ASSERT_EQUAL_INT(0, gbn_sender_init(&s, file10, 2500, 8, 250));
  TEST_ASSERT_EQUAL_size_t(3, gbn_sender_pump(&s, 0, &tx));
  const uint16_t lens[] = {1024, 1024, 452};
  for (uint32_t i = 0; i < 3; i++) {
    struct packet p = tx_packet(&tx, i);
    TEST_ASSERT_EQUAL_UINT8(PKT_DATA, p.type);
    TEST_ASSERT_EQUAL_UINT32(i, p.seq);
    TEST_ASSERT_EQUAL_UINT16(lens[i], p.len);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(file10 + i * 1024, p.payload, p.len);
  }
  struct packet ack = make_ack(3);
  TEST_ASSERT_EQUAL_size_t(1, gbn_sender_on_packet(&s, &ack, 5, &tx));
  struct packet fin = tx_packet(&tx, 0);
  TEST_ASSERT_EQUAL_UINT8(PKT_FIN, fin.type);
  TEST_ASSERT_EQUAL_UINT32(3, fin.seq);
  ack = make_ack(4);
  gbn_sender_on_packet(&s, &ack, 6, &tx);
  TEST_ASSERT_EQUAL_INT(GBN_DONE, s.status);
  gbn_sender_destroy(&s);
}

void test_sender_exact_multiple_of_1024(void) {
  struct gbn_sender s;
  struct gbn_tx tx;
  fill(file10, 2048, 2);
  TEST_ASSERT_EQUAL_INT(0, gbn_sender_init(&s, file10, 2048, 8, 250));
  TEST_ASSERT_EQUAL_UINT32(2, s.ndata);
  // Two full DATA packets, and no FIN until both are acknowledged.
  TEST_ASSERT_EQUAL_size_t(2, gbn_sender_pump(&s, 0, &tx));
  TEST_ASSERT_EQUAL_UINT16(1024, tx_packet(&tx, 0).len);
  TEST_ASSERT_EQUAL_UINT16(1024, tx_packet(&tx, 1).len);
  struct packet ack = make_ack(1);
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_on_packet(&s, &ack, 1, &tx));
  ack = make_ack(2);
  TEST_ASSERT_EQUAL_size_t(1, gbn_sender_on_packet(&s, &ack, 2, &tx));
  struct packet fin = tx_packet(&tx, 0);
  TEST_ASSERT_EQUAL_UINT8(PKT_FIN, fin.type);
  TEST_ASSERT_EQUAL_UINT32(2, fin.seq);
  gbn_sender_destroy(&s);
}

void test_sender_window_full(void) {
  struct gbn_sender s;
  struct gbn_tx tx;
  TEST_ASSERT_EQUAL_INT(0, gbn_sender_init(&s, file10, sizeof file10, 4, 250));
  TEST_ASSERT_EQUAL_size_t(4, gbn_sender_pump(&s, 0, &tx));
  TEST_ASSERT_EQUAL_UINT32(0, s.base);
  TEST_ASSERT_EQUAL_UINT32(4, s.next);
  // The window is full: nothing more goes out and the timer is not moved.
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_pump(&s, 100, &tx));
  TEST_ASSERT_EQUAL_INT64(150, gbn_sender_wait_ms(&s, 100));
  gbn_sender_destroy(&s);
}

void test_sender_cumulative_ack_slides_several(void) {
  struct gbn_sender s;
  struct gbn_tx tx;
  TEST_ASSERT_EQUAL_INT(0, gbn_sender_init(&s, file10, sizeof file10, 4, 250));
  gbn_sender_pump(&s, 0, &tx);
  // ACK 3 alone covers 0, 1 and 2 (the ACKs for 1 and 2 were lost).
  struct packet ack = make_ack(3);
  TEST_ASSERT_EQUAL_size_t(3, gbn_sender_on_packet(&s, &ack, 100, &tx));
  TEST_ASSERT_EQUAL_UINT32(3, s.base);
  TEST_ASSERT_EQUAL_UINT32(7, s.next);
  for (uint32_t i = 0; i < 3; i++) {
    TEST_ASSERT_EQUAL_UINT32(4 + i, tx_packet(&tx, i).seq);
  }
  TEST_ASSERT_EQUAL_INT64(250, gbn_sender_wait_ms(&s, 100)); // restarted
  gbn_sender_destroy(&s);
}

void test_sender_duplicate_and_bogus_acks_ignored(void) {
  struct gbn_sender s;
  struct gbn_tx tx;
  TEST_ASSERT_EQUAL_INT(0, gbn_sender_init(&s, file10, sizeof file10, 4, 250));
  gbn_sender_pump(&s, 0, &tx);
  struct packet ack = make_ack(2);
  gbn_sender_on_packet(&s, &ack, 50, &tx);
  TEST_ASSERT_EQUAL_UINT32(2, s.base);

  // A duplicate ACK changes nothing, not even the timer.
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_on_packet(&s, &ack, 80, &tx));
  ack = make_ack(1);
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_on_packet(&s, &ack, 80, &tx));
  TEST_ASSERT_EQUAL_UINT32(2, s.base);
  TEST_ASSERT_EQUAL_INT64(220, gbn_sender_wait_ms(&s, 80));

  // An ACK for a packet never sent, and a non-ACK, are ignored too.
  ack = make_ack(s.next + 1);
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_on_packet(&s, &ack, 80, &tx));
  struct packet data = make_data(3, "x");
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_on_packet(&s, &data, 80, &tx));
  TEST_ASSERT_EQUAL_UINT32(2, s.base);
  gbn_sender_destroy(&s);
}

void test_sender_timeout_resends_whole_window(void) {
  struct gbn_sender s;
  struct gbn_tx tx;
  TEST_ASSERT_EQUAL_INT(0, gbn_sender_init(&s, file10, sizeof file10, 4, 250));
  gbn_sender_pump(&s, 0, &tx);
  struct packet ack = make_ack(2);
  gbn_sender_on_packet(&s, &ack, 100, &tx); // base 2, next 6, due at 350

  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_on_tick(&s, 349, &tx));
  TEST_ASSERT_EQUAL_INT64(0, gbn_sender_wait_ms(&s, 400));
  TEST_ASSERT_EQUAL_size_t(4, gbn_sender_on_tick(&s, 400, &tx));
  for (uint32_t i = 0; i < 4; i++) {
    struct packet p = tx_packet(&tx, i);
    TEST_ASSERT_EQUAL_UINT32(2 + i, p.seq);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(file10 + (2 + i) * 1024, p.payload, 1024);
  }
  TEST_ASSERT_EQUAL_UINT32(1, s.timeouts);
  TEST_ASSERT_EQUAL_INT64(250, gbn_sender_wait_ms(&s, 400));

  // Progress resets the count of timeouts in a row.
  ack = make_ack(3);
  gbn_sender_on_packet(&s, &ack, 500, &tx);
  TEST_ASSERT_EQUAL_UINT32(0, s.timeouts);
  gbn_sender_destroy(&s);
}

void test_sender_gives_up_after_10_timeouts(void) {
  struct gbn_sender s;
  struct gbn_tx tx;
  TEST_ASSERT_EQUAL_INT(0, gbn_sender_init(&s, file10, 100, 1, 250));
  TEST_ASSERT_EQUAL_size_t(1, gbn_sender_pump(&s, 0, &tx));
  uint64_t now = 0;
  for (int i = 1; i < 10; i++) {
    now += 250;
    TEST_ASSERT_EQUAL_size_t(1, gbn_sender_on_tick(&s, now, &tx));
    TEST_ASSERT_EQUAL_INT(GBN_RUNNING, s.status);
  }
  now += 250;
  TEST_ASSERT_EQUAL_size_t(0, gbn_sender_on_tick(&s, now, &tx));
  TEST_ASSERT_EQUAL_INT(GBN_FAILED, s.status);
  TEST_ASSERT_EQUAL_UINT32(10, s.timeouts);
  TEST_ASSERT_EQUAL_INT64(-1, gbn_sender_wait_ms(&s, now));
  gbn_sender_destroy(&s);
}

void test_sender_stop_and_wait(void) {
  struct gbn_sender s;
  struct gbn_tx tx;
  TEST_ASSERT_EQUAL_INT(0, gbn_sender_init(&s, file10, 3000, 1, 250));
  TEST_ASSERT_EQUAL_size_t(1, gbn_sender_pump(&s, 0, &tx));
  for (uint32_t i = 1; i <= 3; i++) {
    struct packet ack = make_ack(i);
    TEST_ASSERT_EQUAL_size_t(1, gbn_sender_on_packet(&s, &ack, i, &tx));
    TEST_ASSERT_EQUAL_UINT32(i, tx_packet(&tx, 0).seq);
  }
  TEST_ASSERT_EQUAL_UINT8(PKT_FIN, tx_packet(&tx, 0).type);
  gbn_sender_destroy(&s);
}

/* ------------------------------------------------------------------------ */
/* Layer 2: receiver                                                         */
/* ------------------------------------------------------------------------ */

void test_receiver_in_order_duplicate_and_gap(void) {
  struct gbn_receiver r;
  struct gbn_rx rx;
  gbn_receiver_init(&r, 0);

  struct packet p0 = make_data(0, "abc");
  gbn_receiver_on_packet(&r, &p0, 1, &rx);
  TEST_ASSERT_EQUAL_size_t(3, rx.data_len);
  TEST_ASSERT_EQUAL_MEMORY("abc", rx.data, 3);
  TEST_ASSERT_FALSE(rx.finished);
  assert_ack(&rx, 1);

  // A duplicate is discarded and re-acknowledged.
  gbn_receiver_on_packet(&r, &p0, 2, &rx);
  TEST_ASSERT_EQUAL_size_t(0, rx.data_len);
  assert_ack(&rx, 1);

  // A packet from beyond a gap is discarded too.
  struct packet p3 = make_data(3, "zzz");
  gbn_receiver_on_packet(&r, &p3, 3, &rx);
  TEST_ASSERT_EQUAL_size_t(0, rx.data_len);
  assert_ack(&rx, 1);

  struct packet p1 = make_data(1, "de");
  gbn_receiver_on_packet(&r, &p1, 4, &rx);
  TEST_ASSERT_EQUAL_MEMORY("de", rx.data, 2);
  assert_ack(&rx, 2);
  TEST_ASSERT_EQUAL_UINT32(2, r.expected);

  // An early FIN is just another out-of-order packet.
  struct packet early = make_fin(5);
  gbn_receiver_on_packet(&r, &early, 5, &rx);
  TEST_ASSERT_FALSE(rx.finished);
  assert_ack(&rx, 2);
}

void test_receiver_ignores_acks(void) {
  struct gbn_receiver r;
  struct gbn_rx rx;
  gbn_receiver_init(&r, 0);
  struct packet ack = make_ack(0);
  gbn_receiver_on_packet(&r, &ack, 1, &rx);
  TEST_ASSERT_EQUAL_size_t(0, rx.ack_len);
  TEST_ASSERT_EQUAL_size_t(0, rx.data_len);
  TEST_ASSERT_EQUAL_UINT32(0, r.expected);
}

void test_receiver_fin_linger_and_repeated_fin(void) {
  struct gbn_receiver r;
  struct gbn_rx rx;
  gbn_receiver_init(&r, 0);
  struct packet p0 = make_data(0, "only");
  gbn_receiver_on_packet(&r, &p0, 10, &rx);

  struct packet fin = make_fin(1);
  gbn_receiver_on_packet(&r, &fin, 100, &rx);
  TEST_ASSERT_TRUE(rx.finished);
  TEST_ASSERT_EQUAL_size_t(0, rx.data_len);
  assert_ack(&rx, 2);
  TEST_ASSERT_EQUAL_INT64(2000, gbn_receiver_wait_ms(&r, 100));

  // The FIN's ACK was lost: the repeat gets the same ACK, and is not new.
  gbn_receiver_on_packet(&r, &fin, 600, &rx);
  TEST_ASSERT_FALSE(rx.finished);
  assert_ack(&rx, 2);

  // Nothing is delivered after the FIN, even with the "right" seq.
  struct packet late = make_data(2, "late");
  gbn_receiver_on_packet(&r, &late, 700, &rx);
  TEST_ASSERT_EQUAL_size_t(0, rx.data_len);
  assert_ack(&rx, 2);

  TEST_ASSERT_EQUAL_INT(GBN_RUNNING, gbn_receiver_on_tick(&r, 2099));
  TEST_ASSERT_EQUAL_INT64(1, gbn_receiver_wait_ms(&r, 2099));
  TEST_ASSERT_EQUAL_INT(GBN_DONE, gbn_receiver_on_tick(&r, 2100));
  TEST_ASSERT_EQUAL_INT(GBN_DONE, gbn_receiver_on_tick(&r, 99999));
  TEST_ASSERT_EQUAL_INT64(0, gbn_receiver_wait_ms(&r, 2100));

  // Once done, packets are ignored.
  gbn_receiver_on_packet(&r, &fin, 2200, &rx);
  TEST_ASSERT_EQUAL_size_t(0, rx.ack_len);
}

void test_receiver_empty_file(void) {
  struct gbn_receiver r;
  struct gbn_rx rx;
  gbn_receiver_init(&r, 0);
  struct packet fin = make_fin(0);
  gbn_receiver_on_packet(&r, &fin, 0, &rx);
  TEST_ASSERT_TRUE(rx.finished);
  assert_ack(&rx, 1);
}

void test_receiver_gives_up_when_idle(void) {
  struct gbn_receiver r;
  struct gbn_rx rx;
  gbn_receiver_init(&r, 1000);
  TEST_ASSERT_EQUAL_INT64(GBN_IDLE_MS, gbn_receiver_wait_ms(&r, 1000));
  // A valid packet pushes the deadline back.
  struct packet p0 = make_data(0, "a");
  gbn_receiver_on_packet(&r, &p0, 5000, &rx);
  TEST_ASSERT_EQUAL_INT(GBN_RUNNING, gbn_receiver_on_tick(&r, 31000));
  TEST_ASSERT_EQUAL_INT(GBN_RUNNING, gbn_receiver_on_tick(&r, 34999));
  TEST_ASSERT_EQUAL_INT64(0, gbn_receiver_wait_ms(&r, 35000));
  TEST_ASSERT_EQUAL_INT(GBN_FAILED, gbn_receiver_on_tick(&r, 35000));
  TEST_ASSERT_EQUAL_INT(GBN_FAILED, gbn_receiver_on_tick(&r, 40000));
}

/* ------------------------------------------------------------------------ */
/* End to end through an in-memory channel                                   */
/* ------------------------------------------------------------------------ */

#define CHAN_CAP 1024
#define CHAN_LATENCY_MS 10

struct dgram {
  uint64_t at;
  size_t len;
  uint8_t buf[PKT_MAX_LEN];
};

struct channel {
  size_t count;
  struct dgram q[CHAN_CAP];
  double loss, corrupt, dup;
  uint32_t rng;
  unsigned dropped, corrupted, duplicated;
};

static double chan_rand(struct channel *c) {
  // xorshift32: the same sequence on every platform for a given seed.
  c->rng ^= c->rng << 13;
  c->rng ^= c->rng >> 17;
  c->rng ^= c->rng << 5;
  return (double)c->rng / 4294967296.0;
}

static void chan_push(struct channel *c, uint64_t at, const uint8_t *buf, size_t len) {
  TEST_ASSERT_TRUE(c->count < CHAN_CAP);
  struct dgram *d = &c->q[c->count++];
  d->at = at;
  d->len = len;
  memcpy(d->buf, buf, len);
}

/* Puts a datagram on the channel, damaging it the way the relay does. */
static void chan_send(struct channel *c, uint64_t now, const uint8_t *buf, size_t len) {
  if (chan_rand(c) < c->loss) {
    c->dropped++;
    return;
  }
  if (chan_rand(c) < c->corrupt) {
    uint8_t copy[PKT_MAX_LEN];
    memcpy(copy, buf, len);
    size_t bit = (size_t)(chan_rand(c) * (double)(len * 8));
    copy[bit / 8] ^= (uint8_t)(1u << (bit % 8));
    chan_push(c, now + CHAN_LATENCY_MS, copy, len);
    c->corrupted++;
    return;
  }
  chan_push(c, now + CHAN_LATENCY_MS, buf, len);
  if (chan_rand(c) < c->dup) {
    chan_push(c, now + CHAN_LATENCY_MS, buf, len);
    c->duplicated++;
  }
}

/* Removes the earliest datagram due by now. Returns false if there is none. */
static bool chan_pop(struct channel *c, uint64_t now, struct dgram *out) {
  size_t best = c->count;
  for (size_t i = 0; i < c->count; i++) {
    if (c->q[i].at <= now && (best == c->count || c->q[i].at < c->q[best].at)) {
      best = i;
    }
  }
  if (best == c->count) {
    return false;
  }
  *out = c->q[best];
  memmove(&c->q[best], &c->q[best + 1], (c->count - best - 1) * sizeof c->q[0]);
  c->count--;
  return true;
}

static uint64_t chan_next(const struct channel *c, uint64_t none) {
  uint64_t t = none;
  for (size_t i = 0; i < c->count; i++) {
    if (c->q[i].at < t) {
      t = c->q[i].at;
    }
  }
  return t;
}

static struct channel to_recv, to_send;
static struct dgram dg;

/*
 * Runs a whole transfer between the two state machines with a fake clock,
 * damaging each direction at the given rates, and checks that the bytes
 * delivered match the bytes sent.
 */
static void run_transfer(const uint8_t *data, size_t len, uint32_t window,
                         double rate, uint32_t seed) {
  static uint8_t received[64 * 1024];
  size_t received_len = 0;
  struct gbn_sender s;
  struct gbn_receiver r;
  struct gbn_tx tx;
  struct gbn_rx rx;
  struct packet pkt;

  memset(&to_recv, 0, sizeof to_recv);
  memset(&to_send, 0, sizeof to_send);
  to_recv.loss = to_recv.corrupt = to_recv.dup = rate;
  to_send.loss = to_send.corrupt = to_send.dup = rate;
  to_recv.rng = seed;
  to_send.rng = seed * 2654435761u + 1u;

  uint64_t now = 0;
  TEST_ASSERT_EQUAL_INT(0, gbn_sender_init(&s, data, len, window, 100));
  gbn_receiver_init(&r, now);
  gbn_sender_pump(&s, now, &tx);
  for (size_t i = 0; i < tx.count; i++) chan_send(&to_recv, now, tx.pkt[i], tx.len[i]);

  int steps = 0;
  while ((s.status == GBN_RUNNING || r.status == GBN_RUNNING) && steps++ < 1000000) {
    // Jump the fake clock to the next thing that can happen.
    uint64_t next = UINT64_MAX;
    next = chan_next(&to_recv, next);
    next = chan_next(&to_send, next);
    if (s.status == GBN_RUNNING && gbn_sender_wait_ms(&s, now) >= 0) {
      uint64_t t = now + (uint64_t)gbn_sender_wait_ms(&s, now);
      next = t < next ? t : next;
    }
    if (r.status == GBN_RUNNING) {
      uint64_t t = now + (uint64_t)gbn_receiver_wait_ms(&r, now);
      next = t < next ? t : next;
    }
    now = next > now ? next : now;

    while (chan_pop(&to_recv, now, &dg)) {
      if (r.status != GBN_RUNNING || !pkt_decode(dg.buf, dg.len, &pkt)) {
        continue;
      }
      gbn_receiver_on_packet(&r, &pkt, now, &rx);
      TEST_ASSERT_TRUE(received_len + rx.data_len <= sizeof received);
      if (rx.data_len > 0) {
        memcpy(received + received_len, rx.data, rx.data_len);
        received_len += rx.data_len;
      }
      if (rx.ack_len > 0) chan_send(&to_send, now, rx.ack, rx.ack_len);
    }
    while (chan_pop(&to_send, now, &dg)) {
      if (pkt_decode(dg.buf, dg.len, &pkt)) {
        gbn_sender_on_packet(&s, &pkt, now, &tx);
        for (size_t i = 0; i < tx.count; i++) chan_send(&to_recv, now, tx.pkt[i], tx.len[i]);
      }
    }
    gbn_sender_on_tick(&s, now, &tx);
    for (size_t i = 0; i < tx.count; i++) chan_send(&to_recv, now, tx.pkt[i], tx.len[i]);
    gbn_receiver_on_tick(&r, now);
  }

  TEST_ASSERT_EQUAL_INT(GBN_DONE, s.status);
  TEST_ASSERT_EQUAL_INT(GBN_DONE, r.status);
  TEST_ASSERT_EQUAL_size_t(len, received_len);
  if (len > 0) {
    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, received, len);
  }
  if (rate > 0 && len > 8192) {
    // Make sure the channel really did damage traffic in every way.
    TEST_ASSERT_TRUE(to_recv.dropped + to_send.dropped > 0);
    TEST_ASSERT_TRUE(to_recv.corrupted + to_send.corrupted > 0);
    TEST_ASSERT_TRUE(to_recv.duplicated + to_send.duplicated > 0);
  }
  gbn_sender_destroy(&s);
}

static uint8_t big[40 * 1024 + 123];

void test_transfer_clean_channel(void) {
  fill(big, sizeof big, 3);
  run_transfer(big, sizeof big, 1, 0.0, 1);
  run_transfer(big, sizeof big, 16, 0.0, 1);
  run_transfer(NULL, 0, 8, 0.0, 1);
}

void test_transfer_lossy_channel_seeded(void) {
  fill(big, sizeof big, 4);
  const uint32_t seeds[] = {1, 7, 42, 425, 2026, 31337, 99991, 123456789};
  for (size_t i = 0; i < sizeof seeds / sizeof seeds[0]; i++) {
    run_transfer(big, sizeof big, 8, 0.2, seeds[i]);
    run_transfer(big, sizeof big, 1, 0.2, seeds[i]);
    run_transfer(big, 4096, 16, 0.2, seeds[i]); // exact multiple of 1024
    run_transfer(NULL, 0, 4, 0.2, seeds[i]);    // empty file
  }
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_checksum_rfc1071_example);
  RUN_TEST(test_checksum_odd_length);
  RUN_TEST(test_checksum_empty_and_carry);
  RUN_TEST(test_checksum_catches_every_single_bit_flip);
  RUN_TEST(test_encode_hi_example);
  RUN_TEST(test_encode_ack_example);
  RUN_TEST(test_encode_rejects_bad_input);
  RUN_TEST(test_encode_decode_round_trip_largest);
  RUN_TEST(test_decode_null_and_too_short);
  RUN_TEST(test_decode_length_mismatch);
  RUN_TEST(test_decode_unknown_type_and_reserved);
  RUN_TEST(test_sender_init_rejects_bad_arguments);
  RUN_TEST(test_sender_empty_file_is_one_fin);
  RUN_TEST(test_sender_example_2500_bytes);
  RUN_TEST(test_sender_exact_multiple_of_1024);
  RUN_TEST(test_sender_window_full);
  RUN_TEST(test_sender_cumulative_ack_slides_several);
  RUN_TEST(test_sender_duplicate_and_bogus_acks_ignored);
  RUN_TEST(test_sender_timeout_resends_whole_window);
  RUN_TEST(test_sender_gives_up_after_10_timeouts);
  RUN_TEST(test_sender_stop_and_wait);
  RUN_TEST(test_receiver_in_order_duplicate_and_gap);
  RUN_TEST(test_receiver_ignores_acks);
  RUN_TEST(test_receiver_fin_linger_and_repeated_fin);
  RUN_TEST(test_receiver_empty_file);
  RUN_TEST(test_receiver_gives_up_when_idle);
  RUN_TEST(test_transfer_clean_channel);
  RUN_TEST(test_transfer_lossy_channel_seeded);
  return UNITY_END();
}

```

## Scripts Files
Report generated on 10/12/2026 at 05:49:09


---

## End of Report

SHA-256 Hash of the report: 38831df864757bf51af48e3d807d805766b4fb0a40cb1684ec9cf7e31d9f4e20

Do not edit the generated report. Any changes will be reported as academic dishonesty

---
## GitHub Info
- GitHub repo name: adnanmaksic/cs525-p2
- The repository visibility is public.
- The workflow was triggered by adnanmaksic
