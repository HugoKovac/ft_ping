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
#include <signal.h>
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

    while (len > 1)
    {
        sum += *buf++;
        len -= 2;
    }
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

    signal(SIGINT, sig_int);

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

    int recv_ttl_opt = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_RECVTTL, &recv_ttl_opt, sizeof(recv_ttl_opt)) < 0)
    {
        perror("setsockopt IP_RECVTTL");
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
        unsigned char buf[1024];
        struct iovec iov;
        struct msghdr msg;
        char cbuf[CMSG_SPACE(sizeof(int))];

        memset(&reply_addr, 0, sizeof(reply_addr));
        memset(&iov, 0, sizeof(iov));
        memset(&msg, 0, sizeof(msg));
        memset(cbuf, 0, sizeof(cbuf));

        iov.iov_base = buf;
        iov.iov_len = sizeof(buf);
        msg.msg_name = &reply_addr;
        msg.msg_namelen = sizeof(reply_addr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = cbuf;
        msg.msg_controllen = sizeof(cbuf);

        int n = recvmsg(sockfd, &msg, 0);

        if (n < 0)
        {
            perror("recvmsg");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        int recv_ttl = -1;
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        while (cmsg != NULL)
        {
            if (cmsg->cmsg_level == IPPROTO_IP && cmsg->cmsg_type == IP_TTL)
            {
                memcpy(&recv_ttl, CMSG_DATA(cmsg), sizeof(recv_ttl));
                break;
            }
            cmsg = CMSG_NXTHDR(&msg, cmsg);
        }

        struct icmp *icmp_reply = (struct icmp *)buf;

        if (icmp_reply->icmp_type == ICMP_ECHOREPLY)
        {
            struct timeval *tv_send = (struct timeval *)(buf + sizeof(struct icmp));
            struct timeval now;
            gettimeofday(&now, NULL);
            double rtt_ms = (now.tv_sec - tv_send->tv_sec) * 1000.0 +
                            (now.tv_usec - tv_send->tv_usec) / 1000.0;

            printf("%d bytes from %s: icmp_seq=%u ttl=%d time=%.3f ms\n",
                   n,
                   inet_ntoa(reply_addr.sin_addr),
                   ntohs(icmp_reply->icmp_seq),
                   (recv_ttl >= 0 ? recv_ttl : 0),
                   rtt_ms);
        }
        sleep(1);
    }
    close(sockfd);
    return EXIT_SUCCESS;
}
