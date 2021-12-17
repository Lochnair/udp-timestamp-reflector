#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/ip_icmp.h>

#define SERVER_PORT		62222

struct icmp_timestamp_hdr {
    uint8_t     type;
    uint8_t     code;
    uint16_t    checksum;
    uint16_t    identifier;
    uint16_t    sequence;
    uint32_t    originateTime;
	uint32_t    originateTimeNs;
    uint32_t    receiveTime;
	uint32_t    receiveTimeNs;
    uint32_t    transmitTime;
	uint32_t    transmitTimeNs;
};

struct timespec get_time()
{
	struct timespec time;
	clock_gettime(CLOCK_REALTIME, &time);
	return time;
}

unsigned long get_time_since_midnight_ms()
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);

	return (time.tv_sec % 86400 * 1000000) + time.tv_nsec;
    //return (time.tv_sec % 86400 * 1000) + (time.tv_nsec / 1000000);
}

unsigned short calculateChecksum(void *b, int len)
{
    unsigned short *buf = b;
    unsigned int sum=0;
    unsigned short result;
  
    for ( sum = 0; len > 1; len -= 2 )
        sum += *buf++;
    if ( len == 1 )
        sum += *(unsigned char*)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

int create_listen_addr(struct sockaddr_in6 * server_addr)
{
	memset(server_addr, 0, sizeof(*server_addr));
	server_addr->sin6_addr = in6addr_any;
	server_addr->sin6_family = AF_INET6;
	server_addr->sin6_port = htons(SERVER_PORT);

	return 0;
} 

int create_socket(struct sockaddr_in6 * server_addr)
{
	int socket_fd = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

	if (socket_fd < 0)
	{
		perror("couldn't create socket");
		return -1;
	}

	if (bind(socket_fd, (struct sockaddr *) server_addr, sizeof(*server_addr)) > 0)
	{
		perror("bind() failed");
		return -1;
	}

	return socket_fd;
}

void worker(int sock_fd)
{
	for (;;)
	{
		struct icmp_timestamp_hdr hdr;
		struct sockaddr_in6 remote_addr;
		socklen_t addr_len = sizeof(remote_addr);
		int recv = recvfrom(sock_fd, &hdr, sizeof(hdr), 0, (struct sockaddr*) &remote_addr, &addr_len);
		struct timespec receivedTime = get_time();

		if (recv < 0)
			continue;

		if (hdr.type != ICMP_TIMESTAMP)
		{
			continue;
		}

		int recvChecksum = hdr.checksum;
		hdr.checksum = 0;

		if (calculateChecksum(&hdr, sizeof(hdr)) != recvChecksum)
		{
			continue;
		}

		hdr.type = ICMP_TIMESTAMPREPLY;

		hdr.receiveTime = htonl(receivedTime.tv_sec);
		hdr.receiveTimeNs = htonl(receivedTime.tv_nsec);

		struct timespec transmitTime = get_time();
		hdr.transmitTime = htonl(transmitTime.tv_sec);
		hdr.transmitTimeNs = htonl(transmitTime.tv_nsec);
		hdr.checksum = calculateChecksum(&hdr, sizeof(hdr));

		int t;

		if ((t = sendto(sock_fd, &hdr, sizeof(hdr), 0, (struct sockaddr*) &remote_addr, addr_len)) == -1)
		{
			printf("something wrong: %d\n", t);
			continue;
		}
	}
}

int main(int argc, char **argv)
{
	struct sockaddr_in6 server_addr;
	create_listen_addr(&server_addr);

	int sock_fd = create_socket(&server_addr);
	printf("create socket, start listen\n");

	worker(sock_fd);
	
	return 0;
}
