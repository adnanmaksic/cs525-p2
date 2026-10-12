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
