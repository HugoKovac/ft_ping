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
#include <netdb.h>
#include <errno.h>
#include <math.h>
#include "argp.h"
#include "ping.h"

static int resolve_destination(const char *dest, struct sockaddr_in *addr)
{
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = 0;
    if (inet_pton(AF_INET, dest, &addr->sin_addr) == 1)
        return 0;

    struct addrinfo hints;
    struct addrinfo *res = NULL;
    struct addrinfo *p = NULL;
    int resolved = 0;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;

    int gai = getaddrinfo(dest, NULL, &hints, &res);
    if (gai != 0)
    {
        fprintf(stderr, "cannot resolve %s: %s\n", dest, gai_strerror(gai));
        return -1;
    }
    for (p = res; p != NULL; p = p->ai_next)
    {
        if (p->ai_family == AF_INET && p->ai_addrlen >= sizeof(struct sockaddr_in))
        {
            struct sockaddr_in *sin = (struct sockaddr_in *)p->ai_addr;
            addr->sin_addr = sin->sin_addr;
            resolved = 1;
            break;
        }
    }
    freeaddrinfo(res);
    if (!resolved)
    {
        fprintf(stderr, "no IPv4 address found for %s\n", dest);
        return -1;
    }
    return 0;
}

static void print_ping_header(const char *dest, const struct sockaddr_in *addr)
{
    char dst_ip[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &addr->sin_addr, dst_ip, sizeof(dst_ip)) == NULL)
    {
        perror("inet_ntop");
        exit(EXIT_FAILURE);
    }
    int data_len = 64 - (int)sizeof(struct icmp);
    printf("PING %s (%s): %d data bytes\n", dest, dst_ip, data_len);
}

static int configure_socket(int sockfd, int ttl)
{
    int broadcastEnable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable)) < 0)
    {
        perror("setsockopt");
        return -1;
    }
    if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
    {
        perror("setsockopt");
        return -1;
    }
    int recv_ttl_opt = 1;
    if (setsockopt(sockfd, IPPROTO_IP, IP_RECVTTL, &recv_ttl_opt, sizeof(recv_ttl_opt)) < 0)
    {
        perror("setsockopt IP_RECVTTL");
        return -1;
    }
    return 0;
}

static void run_ping_loop(int sockfd, const struct sockaddr_in *addr,
                          int *transmitted, int *received,
                          double *rtt_min, double *rtt_max,
                          double *rtt_sum, double *rtt_sum_sq)
{
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

        int sent = sendto(sockfd, packet, sizeof(packet), 0, (const struct sockaddr *)addr, sizeof(*addr));
        if (sent == -1)
        {
            close(sockfd);
            perror("sendto");
            exit(EXIT_FAILURE);
        }
        (*transmitted)++;

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
            if (errno == EINTR && stop)
                break;
            if (errno == EINTR)
                continue;
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

            (*received)++;
            if (rtt_ms < *rtt_min) *rtt_min = rtt_ms;
            if (rtt_ms > *rtt_max) *rtt_max = rtt_ms;
            *rtt_sum += rtt_ms;
            *rtt_sum_sq += rtt_ms * rtt_ms;
            printf("%d bytes from %s: icmp_seq=%u ttl=%d time=%.3f ms\n",
                   n,
                   inet_ntoa(reply_addr.sin_addr),
                   ntohs(icmp_reply->icmp_seq),
                   (recv_ttl >= 0 ? recv_ttl : 0),
                   rtt_ms);
        }
        sleep(1);
    }
}

static void print_statistics(const char *dest, int transmitted, int received,
                             double rtt_min, double rtt_max,
                             double rtt_sum, double rtt_sum_sq)
{
    printf("--- %s ping statistics ---\n", dest);
    if (transmitted > 0)
    {
        int loss = (int)(((transmitted - received) * 100) / transmitted);
        printf("%d packets transmitted, %d packets received, %d%% packet loss\n",
               transmitted, received, loss);
    }
    else
    {
        printf("0 packets transmitted, 0 packets received, 0%% packet loss\n");
    }
    if (received > 0)
    {
        double avg = rtt_sum / received;
        double variance = (rtt_sum_sq / received) - (avg * avg);
        if (variance < 0.0) variance = 0.0;
        double stddev = sqrt(variance);
        printf("round-trip min/avg/max/stddev = %.3f/%.3f/%.3f/%.3f ms\n",
               rtt_min, avg, rtt_max, stddev);
    }
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

    if (resolve_destination(dest, &addr) != 0)
    {
        close(sockfd);
        return EXIT_FAILURE;
    }

    print_ping_header(dest, &addr);

    if (configure_socket(sockfd, opt_ttl) != 0)
    {
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    int transmitted = 0;
    int received = 0;
    double rtt_min = 1e9;
    double rtt_max = 0.0;
    double rtt_sum = 0.0;
    double rtt_sum_sq = 0.0;
    run_ping_loop(sockfd, &addr, &transmitted, &received, &rtt_min, &rtt_max, &rtt_sum, &rtt_sum_sq);
    print_statistics(dest, transmitted, received, rtt_min, rtt_max, rtt_sum, rtt_sum_sq);
    close(sockfd);
    return EXIT_SUCCESS;
}
