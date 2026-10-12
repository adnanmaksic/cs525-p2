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
