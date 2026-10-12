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
