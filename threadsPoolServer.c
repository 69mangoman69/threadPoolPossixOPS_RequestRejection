#include "threadfunc.h"

int main(int argc, char** argv) {
	USAGE(argc == 2);
	sethandler(sigint_handler, SIGINT);

	// setup the address we'll bind to
	struct sockaddr_in serverAddr = make_sockaddr_in(AF_INET, htons(PORT),
			inet_addr_(argv[1])
			);

	// setup our socket
	int serverSocket = socket_(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// make the server socket reusable and non-blocking
	int reuse = 1;
	setsockopt_(serverSocket, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int));

	// bind our socket to our address
	bind_(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr));

	// listen for up to 1 connection
	listen_(serverSocket, BACKLOG);

	//thread controll
	sem_t newRequestSem = sem_make(0);
	pthread_mutex_t clientQueueMutex = pthread_mutex_make();
	pthread_mutex_t freeThreadCounterMutex = pthread_mutex_make();
	//MyList* clientQueue = newMyList();
	ClientRequest clientQueue = {0};
	int freeThreadCounter = 0;
	pthread_barrier_t sharedAddrBarier = pthread_barrier_make(2);

	// thread setup
	pthread_t* threads = malloc_(THREAD_COUNT * sizeof(pthread_t));
	pthread_attr_t threadAttr = pthread_attr_make();
	ThreadArgs* threadArgs = malloc_(THREAD_COUNT * sizeof(ThreadArgs));
	for (int i = 0; i < THREAD_COUNT; ++i) {
		ThreadArgs args = {
			.threadNum = i + 1,
			.freeThreadCounter = &freeThreadCounter,
			.clientQueue = &clientQueue,
			.clientQueueMutex = &clientQueueMutex,
			.freeThreadCounterMutex = &freeThreadCounterMutex,
			.newRequestSem = &newRequestSem,
			.sharedAddrBarier = &sharedAddrBarier
		};
		pthread_mutex_lock_(&freeThreadCounterMutex);
		freeThreadCounter++;
		pthread_mutex_unlock_(&freeThreadCounterMutex);
		threadArgs[i] = args;
		pthread_create_(&threads[i], &threadAttr, &threadFunc, &threadArgs[i]);
	}

	// queue requests until we're interrupted
	while (!sigint_received) {
		// accept new client,
		// if we get interrupted we assume it's by SIGINT and we stop the loop
		struct sockaddr_in clientAddr = {0};
		socklen_t clientAddrLen = sizeof(struct sockaddr_in);
		int clientSocket
			= accept(serverSocket, (struct sockaddr*) &clientAddr, &clientAddrLen);
		if (errno == EINTR) {
			break;
		}
		pthread_mutex_lock_(&freeThreadCounterMutex);
		if(freeThreadCounter>0){
			pthread_mutex_unlock_(&freeThreadCounterMutex);
			printf_("Accepted %s\n", inet_ntoa(clientAddr.sin_addr));

			

			// receive client's request message which has their redundant client adress????????
			char requestString[LEN * 2] = {0};
			recv_(clientSocket, requestString, 2 * LEN, 0);

			pthread_mutex_lock_(&clientQueueMutex);
			clientQueue.clientSocket = clientSocket;
			// TODO: remove redundant address, pass sockaddr* to thread in request struct
			strncpy(clientQueue.clientAddr, requestString, LEN - 1);
			strncpy(clientQueue.fileName, requestString + LEN, LEN - 1);
			pthread_mutex_unlock_(&clientQueueMutex);

			printf_("Request: \"%s\"\n", clientQueue.fileName);

			sem_post_(&newRequestSem);
			pthread_barrier_wait_(&sharedAddrBarier);
		}
		else{
			pthread_mutex_unlock_(&freeThreadCounterMutex);
			printf_("Rejected %s\n", inet_ntoa(clientAddr.sin_addr));

			char buf[MAX_BUF + 1] = {0};
			sprintf(buf, "Rejected: No free threads");
			send_(clientSocket, buf, strlen(buf), 0);

			close_(clientSocket);
		}
	}

	// cancel threads and wait for them to finish
	for (int i = 0; i < THREAD_COUNT; ++i) {
		pthread_cancel_(threads[i]);
		pthread_join_(threads[i], NULL);
	}

	// cleanup and exit
	printf_("\n"); // slightly tidier exit

	pthread_barrier_destroy_(&sharedAddrBarier);
	sem_destroy_(&newRequestSem);
	pthread_mutex_destroy_(&clientQueueMutex);
	close_(serverSocket);

	FREE(threadArgs);
	FREE(threads);

	return EXIT_SUCCESS;
}

