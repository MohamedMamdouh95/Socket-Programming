/*
 * TEMPLATE 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../sockwrap.h"
#include "../errlib.h"
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define TIMEOUT 15
#define NAME_LENGTH 30
#define RECEIVING_BUFFER_SIZE 512	/* Buffer length */
#define TRANSMISSION_BUFFER_SIZE 512 /* Buffer length */

#define TCP_LISTEN

int tcp_listen(const char *host, const char *serv, socklen_t *addrlenp);
void service(uint32_t newSocketNumber);
char *prog_name;

int main(int argc, char *argv[])
{

	uint32_t socketNumber, newSocketNumber;
	struct sockaddr_in caddr;
	socklen_t addrlen;

#ifndef TCP_LISTEN
	struct sockaddr_in saddr;
	uint32_t backlog = 2;
	uint16_t port_n, port_h;
#endif

#ifdef TCP_LISTEN
#define LISTENQ 1024
	socklen_t addrlen1;

#endif
	prog_name = argv[0];
	if (argc != 2)
	{
		printf("Usage: %s <port number>\n", prog_name);
		exit(1);
	}

#ifndef TCP_LISTEN
	/* get server port number */
	if (sscanf(argv[1], "%" SCNu16, &port_h) != 1)
		err_sys("Invalid port number");
	port_n = htons(port_h);

	/* create the socket */
	printf("creating socket...\n");
	socketNumber = Socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	printf("done, socket number %u\n", socketNumber);

	/* bind the socket to any local IP address */
	bzero(&saddr, sizeof(saddr));
	saddr.sin_family = AF_INET;
	saddr.sin_port = port_n;
	saddr.sin_addr.s_addr = INADDR_ANY;

	Bind(socketNumber, (struct sockaddr *)&saddr, sizeof(saddr));
	printf("Binding done.\n");

	Listen(socketNumber, backlog);
#endif

#ifdef TCP_LISTEN
	socketNumber = tcp_listen(NULL, argv[1], &addrlen1);
#endif
	Signal(SIGPIPE, SIG_IGN);
	while (1)
	{
		addrlen = sizeof(struct sockaddr_in);
		newSocketNumber = Accept(socketNumber, (struct sockaddr *)&caddr, &addrlen);
		service(newSocketNumber);
	}

	return 0;
}

void service(uint32_t newSocketNumber)
{
	char receivingBuffer[RECEIVING_BUFFER_SIZE];
	char transmissionBuffer[TRANSMISSION_BUFFER_SIZE];
	uint32_t numberOfBytesReadFromFile;
	char fileName[NAME_LENGTH];
	uint32_t fileSize;
	time_t seconds;
	char receivedCharacter = '\0';
	FILE *fp;
	struct stat fileStatus;
	int32_t i = 4, counter = 0;
	fd_set cset;
	struct timeval tval;
	uint32_t numberOfBytesReceived;
	uint32_t totalBytesRead = 0;

	FD_ZERO(&cset);
	FD_SET(newSocketNumber, &cset);

	tval.tv_sec = TIMEOUT;
	tval.tv_usec = 0;

	memset(receivingBuffer, 0, RECEIVING_BUFFER_SIZE);

	while ((read(newSocketNumber, &receivedCharacter, 1)) > 0)
	{
		i = 4;
		counter = 0;
		bzero(fileName, NAME_LENGTH);
		bzero(receivingBuffer, RECEIVING_BUFFER_SIZE);
		receivingBuffer[counter] = receivedCharacter;
		counter++;
		totalBytesRead++;

		/* Receive data from the client */
		while (receivedCharacter != '\n' && totalBytesRead < RECEIVING_BUFFER_SIZE)
		{
			numberOfBytesReceived = Select(FD_SETSIZE, &cset, NULL, NULL, &tval);

			if (numberOfBytesReceived > 0)
			{
				numberOfBytesReceived = readn(newSocketNumber, &receivedCharacter, 1);
				if (numberOfBytesReceived != -1)
				{
					//printf("number of bytes received %d\n", numberOfBytesReceived);
				}
				else
				{
					printf("Error in receiving response\n");
					close(newSocketNumber);
					return;
				}
			}

			else
			{
				printf("No response received after %d seconds\n", TIMEOUT);
				if (write(newSocketNumber, "-ERR\r\n", 7) != 7)
					printf("Write error while replying\n");

				close(newSocketNumber);
				return;
			}

			receivingBuffer[counter] = receivedCharacter;
			counter++;
			totalBytesRead++;
		}

		/*check the received instruction is equal to get or GET */
		if ((receivingBuffer[0] == 'G' || receivingBuffer[0] == 'g') && (receivingBuffer[1] == 'E' || receivingBuffer[1] == 'e') && (receivingBuffer[2] == 'T' || receivingBuffer[0] == 't') && receivingBuffer[counter - 2] == '\r')
		{
			/* save the file name */
			while (receivingBuffer[i] != '\r')
			{
				fileName[i - 4] = receivingBuffer[i];
				i++;
			}
			printf("The file name is %s\n", fileName);

			/*Open file in read mode*/
			fp = fopen(fileName, "r");
			if (fp == NULL)
			{
				printf("ERROR: File %s not found.\n", fileName);
				if (write(newSocketNumber, "-ERR\r\n", 7) != 7)
					printf("Write error while replying\n");

				close(newSocketNumber);
				printf("Connection Closed\n");
				return;
			}
			/*Get file size*/
			/*Move file point at the end of file.*/
			fseek(fp, 0, SEEK_END);

			/*Get the current position of the file pointer.*/
			fileSize = ftell(fp);
			rewind(fp); // reset the pointer to the start of the file

			if (fileSize != -1)
				printf("File size is: %d\n", fileSize);
			else
			{
				printf("There is some ERROR.\n");
				if (write(newSocketNumber, "-ERR\r\n", 7) != 7)
					printf("Write error while replying\n");
				close(newSocketNumber);
				fclose(fp);
				printf("Connection Closed\n");
				return;
			}

			fileSize = htonl(fileSize);

			/*clear the transmission buffer*/
			bzero(transmissionBuffer, TRANSMISSION_BUFFER_SIZE);

			/*Send the server response and file size*/
			if (write(newSocketNumber, "+OK\r\n", 5) != 5)
			{
				printf("Write error while replying\n");
				if (write(newSocketNumber, "-ERR\r\n", 7) != 7)
					printf("Write error while replying\n");
				close(newSocketNumber);
				fclose(fp);
				printf("Connection Closed\n");
				return;
			}

			if (write(newSocketNumber, &fileSize, 4) != 4)
			{
				printf("Write error while replying\n");
				if (write(newSocketNumber, "-ERR\r\n", 7) != 7)
					printf("Write error while replying\n");
				close(newSocketNumber);
				fclose(fp);
				printf("Connection Closed\n");
				return;
			}

			bzero(transmissionBuffer, TRANSMISSION_BUFFER_SIZE);

			/*Read bytes from file and save them in the transmission buffer*/
			while ((numberOfBytesReadFromFile = fread(transmissionBuffer, sizeof(char), TRANSMISSION_BUFFER_SIZE, fp)) > 0)
			{
				/*Send the data saved in the transmission buffer*/
				if (writen(newSocketNumber, transmissionBuffer, numberOfBytesReadFromFile) < 0)
				{
					printf("ERROR: Failed to send file %s.\n", fileName);
					if (write(newSocketNumber, "-ERR\r\n", 7) != 7)
						printf("Write error while replying\n");
					close(newSocketNumber);
					fclose(fp);
					printf("Connection Closed\n");
					return;
				}
				bzero(transmissionBuffer, TRANSMISSION_BUFFER_SIZE);
			}
			/*Read the time stamp then send it in network byte order*/
			stat(fileName, &fileStatus);
			seconds = htonl(fileStatus.st_mtime);
			printf("Time stamp is %d.\n", ntohl(seconds));

			if ((write(newSocketNumber, &seconds, 4)) < 0)
			{
				printf("ERROR: Failed to send time %d.\n", ntohl(seconds));
				if (write(newSocketNumber, "-ERR\r\n", 7) != 7)
					printf("Write error while replying\n");
				close(newSocketNumber);
				fclose(fp);
				printf("Connection Closed\n");
				return;
			}

			bzero(transmissionBuffer, TRANSMISSION_BUFFER_SIZE);
			printf("File transmission done\n\n");
			fclose(fp);
		}
		else
		{
			printf("ERROR: GET command is not properly sent.\n");

			if (write(newSocketNumber, "-ERR\r\n", 7) != 7)
				printf("Write error while replying\n");
			close(newSocketNumber);
			printf("Connection Closed\n");
			return;
		}
		receivedCharacter = '\0';
		totalBytesRead = 0;
	}
	close(newSocketNumber);
	printf("Connection Closed\n");
}

#ifdef TCP_LISTEN

int tcp_listen(const char *host, const char *serv, socklen_t *addrlenp)
{
	int listenfd, n;
	const int on = 1;
	struct addrinfo hints, *res, *ressave;

	bzero(&hints, sizeof(struct addrinfo));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((n = getaddrinfo(host, serv, &hints, &res)) != 0)
		err_quit("tcp_listen error for %s, %s: %s",
				 host, serv, gai_strerror(n));
	ressave = res;

	do
	{
		listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (listenfd < 0)
			continue; /* error, try next one */

		Setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
		if (bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
			break; /* success */

		Close(listenfd); /* bind error, close and try next one */
	} while ((res = res->ai_next) != NULL);

	if (res == NULL) /* errno from final socket() or bind() */
		err_sys("tcp_listen error for %s, %s", host, serv);

	Listen(listenfd, LISTENQ);

	if (addrlenp)
		*addrlenp = res->ai_addrlen; /* return size of protocol address */

	freeaddrinfo(ressave);

	return (listenfd);
}

#endif
