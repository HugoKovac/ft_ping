#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "argp.h"

int volatile stop = 0;

void sig_int(int signal)
{
    (void)signal;
    stop = 1;
}

unsigned short checksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2)
        sum += *buf++;
    if (len == 1)
        sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

static int opt_ttl = 64;

static void print_help(const char *progname)
{
    printf("Usage: %s [OPTIONS] DESTINATION\n", progname);
    printf("Send ICMP ECHO_REQUEST to network hosts.\n\n");
    printf("Options:\n");
    printf("  -t, --ttl=TTL       Set IP TTL (default %d)\n", opt_ttl);
    printf("  -?, --help          Display this help and exit\n");
}

static int parse_opt(int key, const char *arg, struct argp_state *state)
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

int main(int argc, char **argv)
{
    const struct argp_option options[] = {
        {"ttl", 't', ARGP_REQUIRED_ARG, "Set IP TTL", 0},
        {"help", '?', ARGP_NO_ARG, "Display this help", 0},
        {NULL, 0, 0, NULL, 0}};

    const struct argp argp = {options, parse_opt, "DEST", "ft_ping: send ICMP ECHO_REQUEST to network hosts"};

    int arg_index = 0;
    if (argp_parse(&argp, argc, argv, &arg_index, NULL) != ARGP_SUCCESS)
    {
        return EXIT_FAILURE;
    }

    if (arg_index >= argc)
    {
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    const char *dest = argv[arg_index];

    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP); // DGRAM doesn't require root privileges
    if (sockfd == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = 0;
    if (inet_pton(AF_INET, dest, &addr.sin_addr) != 1)
    {
        fprintf(stderr, "Invalid destination address: %s\n", dest);
        close(sockfd);
        return EXIT_FAILURE;
    }

    int broadcastEnable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0)
    {
        perror("setsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &opt_ttl, sizeof(opt_ttl)) < 0)
    {
        perror("setsockopt");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int seq = 0;
    while (!stop)
    {
        unsigned char packet[64];
        struct icmp *icmp_hdr = (struct icmp *)packet;
        struct timeval *tv = (struct timeval *)(packet + sizeof(struct icmp));

        memset(packet, 0, sizeof(packet));
        
        icmp_hdr->icmp_type = ICMP_ECHO;
        icmp_hdr->icmp_code = 0;
        icmp_hdr->icmp_cksum = 0;
        icmp_hdr->icmp_hun.ih_idseq.icd_id = htons((uint16_t)(getpid() & 0xFFFF));
        icmp_hdr->icmp_hun.ih_idseq.icd_seq = htons(seq++);
        
        gettimeofday(tv, NULL);
        
        icmp_hdr->icmp_cksum = checksum(packet, sizeof(packet));

        int sent = sendto(sockfd, packet, sizeof(packet), 0, (struct sockaddr *)&addr, sizeof(addr));
        if (sent == -1)
        {
            close(sockfd);
            perror("sendto");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in reply_addr;
        socklen_t addrlen = sizeof(reply_addr);
        unsigned char buf[1024];

        int n = recvfrom(sockfd, buf, sizeof(buf), 0,
                         (struct sockaddr *)&reply_addr, &addrlen);

        if (n < 0)
        {
            perror("recvfrom");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        struct ip *ip_hdr = (struct ip *)buf;
        int iphdrlen = ip_hdr->ip_hl * 4;

        struct icmp *icmp_reply = (struct icmp *)(buf + iphdrlen);

        if (icmp_reply->icmp_type == ICMP_ECHOREPLY)
        {
            
            printf("%zu bytes from %s: icmp_seq=%u ttl=%d time=%.3f ms\n",
                (size_t)(n - iphdrlen),
                inet_ntoa(*(struct in_addr *)&reply_addr.sin_addr.s_addr),
                ntohs(icmp_reply->icmp_seq),
                ip_hdr->ip_ttl, (double)(tv->tv_usec - icmp_hdr->icmp_hun.ih_idseq.icd_id) / 1000.0);
        }
        sleep(1);
    }
    close(sockfd);
    return EXIT_SUCCESS;
}
