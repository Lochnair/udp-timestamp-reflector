#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>

#define LISTEN_THREADS	4
#define SERVER_PORT		62222

typedef struct {
    uint8_t type;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
    uint32_t originalTime;
    uint32_t receiveTime;
    uint32_t transmitTime;
} udpTsHdr;

typedef struct {
	struct sockaddr_in6 * server_addr;
	int threadId;
} worker_data;

unsigned long get_time_since_midnight_ms()
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return (time.tv_sec % 86400 * 1000) + (time.tv_nsec / 1000000);
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
	int optval = 1;

	if (socket_fd < 0)
	{
		perror("couldn't create socket");
		return -1;
	}


	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) > 0)
	{
		perror("couldn't set reuseport");
		return -1;
	}

	if (bind(socket_fd, (struct sockaddr *) server_addr, sizeof(*server_addr)) > 0)
	{
		perror("bind() failed");
		return -1;
	}

	return socket_fd;
}

void * worker(void * data)
{
	worker_data * workerData = (worker_data *) data;
	int socket_fd = create_socket(workerData->server_addr);

	printf("[%d] create socket, start listen\n", workerData->threadId);

	for (;;)
	{
		udpTsHdr hdr;
		struct sockaddr_in6 remote_addr;
		socklen_t addr_len = sizeof(remote_addr);

		printf("[%d] wait for packet\n", workerData->threadId);
		int recv = recvfrom((int)socket_fd, &hdr, sizeof(hdr), 0, (struct sockaddr*) &remote_addr, &addr_len);
		int receivedTime = htonl(get_time_since_midnight_ms());
		printf("[%d] Received: %d bytes\n", workerData->threadId, recv);

		if (hdr.type != 0x5D)
		{
			printf("[%d] not a timestamp request", workerData->threadId);
			continue;
		}

		int recvChecksum = hdr.checksum;
		hdr.checksum = 0;

		if (calculateChecksum(&hdr, sizeof(hdr)) != recvChecksum)
		{
			printf("[%d] wrong chksum", workerData->threadId);
			continue;
		}

		hdr.receiveTime = receivedTime;
		hdr.transmitTime = htonl(get_time_since_midnight_ms());

		hdr.checksum = calculateChecksum(&hdr, sizeof(hdr));

		int t;

		if ((t = sendto(socket_fd, &hdr, sizeof(hdr), 0, (struct sockaddr*) &remote_addr, addr_len)) == -1)
		{
			printf("[%d] something wrong: %d\n",workerData->threadId, t);
			continue;
		}
	}
}

int main(int argc, char **argv)
{
	pthread_t listenThreads[LISTEN_THREADS];

	printf("hello world\n");
	struct sockaddr_in6 server_addr;
	create_listen_addr(&server_addr);

	for (int t = 0; t < LISTEN_THREADS; t++)
	{
		int ret;
		pthread_t thread;
		worker_data data;

		data.server_addr = &server_addr;
		data.threadId = t;

		printf("creating thhread: %d -> %d\n", data.threadId, t);

		if ((ret = pthread_create(&thread, NULL, worker, (void *) &data)) != 0)
		{
			printf("failed to create listen thread: %d\n", ret);
		}

		listenThreads[t] = thread;
	}

	for (int t = 0; t < LISTEN_THREADS; t++)
	{
		int thread;
		void *status;

		printf("joining thhread: %d\n", t);

		if ((thread = pthread_join(listenThreads[t], &status)) != 0)
		{
			printf("Error in listen thread join: %d\n", thread);
		}
	}
	
	return 0;
}
