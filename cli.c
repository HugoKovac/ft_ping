#include <stdio.h>
#include <stdlib.h>
#include "ping.h"

int opt_ttl = 64;

void print_help(const char *progname)
{
    printf("Usage: %s [OPTIONS] DESTINATION\n", progname);
    printf("Send ICMP ECHO_REQUEST to network hosts.\n\n");
    printf("Options:\n");
    printf("  -t, --ttl=TTL       Set IP TTL (default %d)\n", opt_ttl);
    printf("  -?, --help          Display this help and exit\n");
}

int parse_opt(int key, const char *arg, struct argp_state *state)
{
    (void)state;
    (void)arg;
    switch (key)
    {
    case 't':
        if (!arg)
        {
            fprintf(stderr, "--ttl requires a value\n");
            return ARGP_ERR_ARG;
        }
        opt_ttl = atoi(arg);
        if (opt_ttl <= 0 || opt_ttl > 255)
        {
            fprintf(stderr, "invalid ttl: %s\n", arg);
            return ARGP_ERR_ARG;
        }
        break;
    case '?':
        print_help(state->argv[0]);
        exit(0);
        break;
    default:
        return 0;
    }
    return 0;
}


