#define ERR_MULTIPROCESS 0
#define USAGE_STRING "serverAddress clientAddress fileName"
#include "katwikOpsys.h"

#define PORT 3500

// TODO: this naming convention ain't so sexy lol
#define MAX_BUF 500
#define LEN 40

int main(int argc, char** argv) {
	USAGE(argc == 4);

	// setup the address we'll connect to
	struct sockaddr_in serverAddr = make_sockaddr_in( AF_INET, htons(PORT),
			inet_addr_(argv[1])
			);

	// setup the address we'll connect to
	struct sockaddr_in clientAddr = make_sockaddr_in( AF_INET, htons(PORT),
			inet_addr_(argv[2])
			);

	// setup our socket, we'll also use this to connect
	int clientSock = socket_(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// make the server socket reusable and non-blocking
	int reuse = 1;
	setsockopt_(clientSock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

	// bind our socket to our address
	bind_(clientSock, (struct sockaddr*) &clientAddr, sizeof(clientAddr));

	// conect to the server through our socket
	ERR_NEG1(connect(clientSock, (struct sockaddr*) &serverAddr, sizeof(serverAddr)));

	// preparing to send message
	// TODO: kek
	// the server knows the address as soon as they accept the connection lmao
	// no need to send it as string
	char request[LEN * 2] = {0};
	snprintf(request, LEN - 1, "%s", argv[2]);
	snprintf(request + LEN, LEN - 1, "%s", argv[3]);
	send_(clientSock, &request, sizeof(request), 0);

	// receive something from the server
	char recvBuf[MAX_BUF + 1] = {0};
	recv_(clientSock, recvBuf, MAX_BUF, 0);
	printf_("Received:\n%s\n", recvBuf);

	// cleanup and exit
	close_(clientSock);

	return EXIT_SUCCESS;
}

