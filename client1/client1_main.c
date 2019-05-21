/*
 * TEMPLATE 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "../sockwrap.h"
#include "../errlib.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <unistd.h>

#define TCP_CONNECT

#define TIMEOUT 15
#define RECEIVING_BUFFER_SIZE 4096

char *prog_name;
int tcp_connect(const char *host, const char *serv);
int myread(int socketNumber, void *receivingBuffer, size_t nbytes);

int main(int argc, char *argv[])
{
#ifndef TCP_CONNECT
	uint32_t address;
	uint16_t port_n, port_h;
	struct sockaddr_in saddr;
#endif
	uint32_t socketNumber;

	char receivingBuffer[RECEIVING_BUFFER_SIZE];

	char *fileName;
	FILE *fp;
	int32_t fileSize = 0;
	uint32_t numberOfBytesReceived;
	int32_t remainingBytes;
	time_t timeStamp[2];
	uint32_t fileIndicator = 0;

	fd_set cset;
	struct timeval tval;

	prog_name = argv[0];
	if (argc < 4)
	{
		err_quit("usage: %s <dest_host> <dest_port> <filename>", prog_name);
	}

#ifndef TCP_CONNECT
	address = inet_addr(argv[1]);

	port_h = atoi(argv[2]);
	port_n = htons(port_h);

	/*create socket*/
	socketNumber = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	//printf("done. Socket fd number: %d\n", socketNumber);

	/*Fill the server address struct*/
	bzero(&saddr, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = port_n;
	saddr.sin_addr.s_addr = address;

	/*connect to the TCP server*/
	Connect(socketNumber, (struct sockaddr *)&saddr, (socklen_t)sizeof(saddr));
	//exit(0);
#endif

#ifdef TCP_CONNECT
	socketNumber = tcp_connect(argv[1], argv[2]);
#endif
	//printf("Connected successfully to the server\n");
	while (fileIndicator < argc - 3)
	{
		fileName = argv[3 + fileIndicator];

		/*send the get request*/
		Send(socketNumber, "GET ", 4, 0);
		Send(socketNumber, fileName, strlen(fileName), 0);
		Send(socketNumber, "\r\n", 2, 0);

		bzero(receivingBuffer, RECEIVING_BUFFER_SIZE);

		FD_ZERO(&cset);
		FD_SET(socketNumber, &cset);

		tval.tv_sec = TIMEOUT;
		tval.tv_usec = 0;

		numberOfBytesReceived = Select(FD_SETSIZE, &cset, NULL, NULL, &tval);

		if (numberOfBytesReceived > 0)
		{
			numberOfBytesReceived = readn(socketNumber, receivingBuffer, 5);
			if (numberOfBytesReceived == 0)
			{
				printf("Connection closed or Server is down\n");
				return 0;
			}
			else if (numberOfBytesReceived != -1)
			{
				//printf("number of bytes received %d 1\n", numberOfBytesReceived);
			}
			else
			{
				printf("Error in receiving response\n");
				return 0;
			}
		}

		else
		{
			printf("No response received after %d seconds\n", TIMEOUT);
			return 0;
		}

		if (receivingBuffer[0] == '+' && receivingBuffer[1] == 'O' && receivingBuffer[2] == 'K' && receivingBuffer[3] == '\r' && receivingBuffer[4] == '\n')
		{
			FD_ZERO(&cset);
			FD_SET(socketNumber, &cset);
			tval.tv_sec = TIMEOUT;
			tval.tv_usec = 0;

			/*Create new file to save the data in*/
			fp = fopen(fileName, "w");

			if (fp == NULL)
			{
				printf("Cannot open file\n");
			}

			numberOfBytesReceived = Select(FD_SETSIZE, &cset, NULL, NULL, &tval);

			if (numberOfBytesReceived > 0)
			{
				numberOfBytesReceived = readn(socketNumber, &fileSize, 4);

				if (numberOfBytesReceived == 0)
				{
					fclose(fp);
					printf("Connection closed or Server is down\n");
					return 0;
				}
				else if (numberOfBytesReceived != -1)
				{
					//printf("number of bytes received %d 1\n", numberOfBytesReceived);
				}
				else
				{
					printf("Error in receiving response\n");
					return 0;
				}
			}

			else
			{
				printf("No response received after %d seconds\n", TIMEOUT);
				return 0;
			}

			fileSize = ntohl(fileSize);
			bzero(receivingBuffer, RECEIVING_BUFFER_SIZE);
			remainingBytes = fileSize;

			while (remainingBytes > 0)
			{
				FD_ZERO(&cset);
				FD_SET(socketNumber, &cset);
				tval.tv_sec = TIMEOUT;
				tval.tv_usec = 0;

				numberOfBytesReceived = Select(FD_SETSIZE, &cset, NULL, NULL, &tval);

				if (remainingBytes < RECEIVING_BUFFER_SIZE)
				{
					if (numberOfBytesReceived > 0)
					{
						numberOfBytesReceived = read(socketNumber, receivingBuffer, remainingBytes);
						if (numberOfBytesReceived == 0)
						{
							fclose(fp);
							printf("Connection closed or Server is down\n");
							return 0;
						}
						else if (numberOfBytesReceived != -1)
						{
							//printf("number of bytes received %d 1\n", numberOfBytesReceived);
						}
						else
						{
							printf("Error in receiving response\n");
							return 0;
						}
					}

					else
					{
						printf("No response received after %d seconds\n", TIMEOUT);
						return 0;
					}
				}
				else
				{
					if (numberOfBytesReceived > 0)
					{
						numberOfBytesReceived = read(socketNumber, receivingBuffer, RECEIVING_BUFFER_SIZE);
						if (numberOfBytesReceived == 0)
						{
							fclose(fp);
							printf("Connection closed or Server is down\n");
							return 0;
						}
						else if (numberOfBytesReceived != -1)
						{
							//printf("number of bytes received %d 1\n", numberOfBytesReceived);
						}
						else
						{
							printf("Error in receiving response\n");
							return 0;
						}
					}

					else
					{
						printf("No response received after %d seconds\n", TIMEOUT);
						return 0;
					}
				}
				remainingBytes = remainingBytes - numberOfBytesReceived;

				fwrite(receivingBuffer, sizeof(char), numberOfBytesReceived, fp);
				bzero(receivingBuffer, RECEIVING_BUFFER_SIZE);
			}
			printf("Received File %s\n", fileName);
			printf("Received File size %d\n", fileSize);

			FD_ZERO(&cset);
			FD_SET(socketNumber, &cset);
			tval.tv_sec = TIMEOUT;
			tval.tv_usec = 0;

			numberOfBytesReceived = Select(FD_SETSIZE, &cset, NULL, NULL, &tval);

			if (numberOfBytesReceived > 0)
			{
				numberOfBytesReceived = read(socketNumber, timeStamp, sizeof(timeStamp));
				if (numberOfBytesReceived == 0)
				{
					fclose(fp);
					printf("Connection closed or Server is down\n");
					return 0;
				}
				else if (numberOfBytesReceived != -1)
				{
					//printf("number of bytes received %d 1\n", numberOfBytesReceived);
				}
				else
				{
					printf("Error in receiving response\n");
					return 0;
				}
			}

			else
			{
				printf("No response received after %d seconds\n", TIMEOUT);
				return 0;
			}

			timeStamp[0] = ntohl(timeStamp[0]);
			printf("Received File timestamp %ld\n\n", timeStamp[0]);
			if (numberOfBytesReceived > 4)
			{
				printf("ERROR: Server sent more data than expected\n");
				close(socketNumber);
				return 0;
			}
			fclose(fp);
			fileIndicator++;
		}
		else
		{
			printf("Error in file Reception\n");
			close(socketNumber);
			return 0;
		}
	}

	close(socketNumber);

	return 0;
}

#ifdef TCP_CONNECT

int tcp_connect(const char *host, const char *serv)
{
	int sockfd, n;
	struct addrinfo hints, *res, *ressave;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((n = getaddrinfo(host, serv, &hints, &res)) != 0)
		err_quit("tcp_connect error for %s, %s: %s",
				 host, serv, gai_strerror(n));
	ressave = res;

	do
	{
		sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (sockfd < 0)
			continue; /* ignore this one */

		if (connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
			break; /* success */

		Close(sockfd); /* ignore this one */
	} while ((res = res->ai_next) != NULL);

	if (res == NULL) /* errno set from final connect() */
		err_sys("tcp_connect error for %s, %s", host, serv);

	freeaddrinfo(ressave);

	return (sockfd);
}

#endif
