/* file : monitorServer.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <pthread.h>
#include "input_check_server.h"
#include "m_helper.h"
#include "m_threads.h"
#include "messages.h"

pthread_mutex_t mutex;			// mutex used to protect shared monitor server data among threads
pthread_cond_t not_empty;		// condition variable used to wait until cyclic buffer is not empty
pthread_cond_t not_full;		// condition variable used to wait until cyclic buffer is not full

int main(int argc, char const *argv[])
{

	srand((unsigned int)time(NULL));
	int socketBufferSize, cyclicBufferSize, numThreads, socket_fd;
	unsigned int bloom_size;
	in_port_t port;

	/* check for correct arg input execv and initialize program parameters */
	if (!check_init_args_server(argc, argv, &port, &numThreads, &socketBufferSize, &cyclicBufferSize, &bloom_size))
		exit(EXIT_FAILURE);

	/* initialization phase (part1) */
	if ((socket_fd = setup_server(port)) < 0)
		exit(EXIT_FAILURE);
	
	/* initialization phase (part2) */
	struct Monitor * monitor = Monitor_init(socketBufferSize, numThreads, bloom_size, 8, 0.5); 	// initialize structures kept by Monitor
	monitor->socket_fd = socket_fd;
	struct cyclicBuffer * buffer = cyclicBuffer_init(cyclicBufferSize);			// initialize cyclic buffer size struct
	
	if (mutex_cond_init(&mutex, &not_empty, &not_full) < 0)					// initialize the mutex and the condition variables
		exit(EXIT_FAILURE);

	/* initialization phase (part3) */
	create_threads(monitor, buffer);		/* create the helper threads and assign them to read the input country files that the monitor server puts in cyclic buffer */
	// place all countries files into the cyclic buffer, other threads read the files from buffer and update the shared monitor structures
	producer_thread(monitor, buffer, &argv[11]);	// after reading subdirectories and setting up bloom filters, one of the threads sends them back to the travelMonitorClient

	void * message;
	int msgd;
	char subdir[30] = "";

	while(1)
	{

		message = read_message(socket_fd, &msgd, socketBufferSize);		// main thread will wait here until it reads message

		if (Monitor_take_action(monitor, buffer, msgd, message, subdir) < 0)	// take action depending on the message descriptor and the message just read
		{
			fprintf(stderr, "[Error] : Monitor -> Monitor_take_action : Invalid message descriptor\n");
			exit(EXIT_FAILURE);
		}

		// repeat until client sends EXIT message
	}

}