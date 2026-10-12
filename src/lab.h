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
