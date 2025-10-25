#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <netinet/icmp6.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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

int main()
{
    printf("Hello ping!\n");
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
    inet_pton(AF_INET, "8.8.8.8", &addr.sin_addr);
    
    if (setsockopt(sockfd, IPPROTO_IP, IP_TTL, &addr, sizeof(addr)) < 0){
        perror("setsockopt");
    }
    
    int seq = 0;
    while (!stop)
    {
        struct icmp icmp_hdr;
    
        icmp_hdr.icmp_type = ICMP_ECHO;
        icmp_hdr.icmp_code = 0;
        icmp_hdr.icmp_cksum = 0;
        icmp_hdr.icmp_hun.ih_idseq.icd_id = htons((uint16_t)(getpid() & 0xFFFF));
        icmp_hdr.icmp_hun.ih_idseq.icd_seq = htons(seq++);
        icmp_hdr.icmp_cksum = checksum(&icmp_hdr, sizeof(icmp_hdr));

        int sent = sendto(sockfd, &icmp_hdr, sizeof(icmp_hdr), 0, (struct sockaddr *)&addr, sizeof(addr));
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

        if (n < iphdrlen + (int)sizeof(struct icmp))
        {
            fprintf(stderr, "Received packet too short (%d bytes)\n", n);
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        struct icmp *icmp_reply = (struct icmp *)(buf + iphdrlen);

        if (icmp_reply->icmp_type == ICMP_ECHOREPLY)
        {
            printf("id=%d seq=%d\n",
                    ntohs(icmp_reply->icmp_hun.ih_idseq.icd_id),
                    ntohs(icmp_reply->icmp_hun.ih_idseq.icd_seq));
        }
        else
        {
            printf("Reçu autre type ICMP : %d\n", icmp_reply->icmp_type);
        }
        sleep(1);
    }
    close(sockfd);
}
