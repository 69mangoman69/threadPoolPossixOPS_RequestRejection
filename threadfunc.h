
#define ERR_MULTIPROCESS 0
#define USAGE_STRING "serverAddress"

typedef struct ClientRequest_ ClientRequest;
#define LIST_TYPE ClientRequest*
#include "katwikOpsys.h"

#define PORT 3500
#define BACKLOG 1

#define MAX_BUF 500
#define LEN 40
#define THREAD_COUNT 2

typedef struct ThreadArgs_ {
	int threadNum;
	int* freeThreadCounter;
	pthread_mutex_t* clientQueueMutex;
	pthread_mutex_t* freeThreadCounterMutex;
	ClientRequest* clientQueue;
	sem_t* newRequestSem;
	pthread_barrier_t* sharedAddrBarier;
} ThreadArgs;

struct ClientRequest_ {
	char clientAddr[LEN];
	char fileName[LEN];
	int clientSocket;
};

volatile sig_atomic_t sigint_received = 0;
void sigint_handler(int sig) {
	UNUSED(sig);
	sigint_received = 1;
}

void* threadFunc(void* voidArgs) {
	pthread_setcanceltype_(PTHREAD_CANCEL_DEFERRED, NULL);
	ThreadArgs* args = (ThreadArgs*) voidArgs;

	while (!sigint_received) {
		// wait until we're signaled for a new request
		sem_wait_(args->newRequestSem);
        sleep(2);
		pthread_mutex_lock_(args->freeThreadCounterMutex);
		(*(args->freeThreadCounter))--;
		pthread_mutex_unlock_(args->freeThreadCounterMutex);

		// get the request
		pthread_mutex_lock_(args->clientQueueMutex);
		ClientRequest tmpClientRequest = {.clientSocket = args->clientQueue->clientSocket};
		strcpy(tmpClientRequest.clientAddr, args->clientQueue->clientAddr);
		strcpy(tmpClientRequest.fileName, args->clientQueue->fileName);
		pthread_mutex_unlock_(args->clientQueueMutex);

		pthread_barrier_wait_(args->sharedAddrBarier);
        sleep(3);
		printf_("Thread %d handling request: \"%s\" from %s\n",
				args->threadNum, tmpClientRequest.fileName, tmpClientRequest.clientAddr);
		
		// open the requested file
		int filedes = open_(tmpClientRequest.fileName, O_RDONLY);
		// TODO: handle ENOENT here lol

		// read the file and send it
		char buf[MAX_BUF + 1] = {0};
		read_(filedes, buf, MAX_BUF);
		send_(tmpClientRequest.clientSocket, buf, strlen(buf), 0);

		// cleanup
		close_(tmpClientRequest.clientSocket);

		pthread_mutex_lock_(args->freeThreadCounterMutex);
		(*(args->freeThreadCounter))++;
		pthread_mutex_unlock_(args->freeThreadCounterMutex);
	}

	return NULL;
}