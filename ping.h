// Simple shared declarations for ft_ping
#ifndef FT_PING_H
#define FT_PING_H

#include "argp.h"

// Signal/loop control
extern volatile int stop;
void sig_int(int signal);

// ICMP helpers
unsigned short checksum(void *b, int len);

// CLI options
extern int opt_ttl;
void print_help(const char *progname);
int parse_opt(int key, const char *arg, struct argp_state *state);

#endif


