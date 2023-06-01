/* file : m_helper.h */
/* important helper functions and structs for Monitor are developed here */
#pragma once
#include "hash.h"
#include <netinet/in.h>
#include <pthread.h>
#include "m_threads.h"

struct cyclicBuffer;

struct Monitor {
	int accepted;
	int rejected;
	int bufferSize;
	int socket_fd;
	int numThreads;
	HT citizens_info;
	HT viruses_info;
	HT countries_info;
	unsigned int bloom_size;
	int max_level;
	float p;
	pthread_t * tids;	// a table of thread ids

};

/*__________________________________________________________________________*/


/*===================== INITIALIZATION PHASE ===========================*/

/* initializes the monitor structure and all its substructures needed */
struct Monitor * Monitor_init(int bufferSize, int numThreads, unsigned int bloom_size, int max_level, float p);
/* setups server, calls socket, bind, listen, accept, returns the socket_fd or -1 on failure */
int setup_server(in_port_t port);
/* sends back the bloom filters to the travelMonitorClient */
void send_bloom_filters(struct Monitor * monitor);
/* inserts given entry/line from file into all the necessary data structures of the monitor */
void Monitor_insert(struct Monitor * monitor, char * citizenID , char * firstName, char * lastName, char * country, unsigned int age, char * virusName, char * vacc, char * date);


/* =================== QUERY PHASE ========================= */

/* monitor process takes an action depending on the message it received */
int Monitor_take_action(struct Monitor * monitor, struct cyclicBuffer * buffer, int msgd, void * message, char * subdir);
void vaccineStatus(struct Monitor * monitor, char * citizenID, char * virusName);
// upon receiving EXIT message, monitorServer prints to a log file, cleans up data structures and exits
void exit_monitorServer(struct Monitor * monitor, struct cyclicBuffer * buffer);

/*==================== EXIT PHASE ========================== */
/* destroys the monitor structure and all of its substructures that were created and used, closes open file descriptors */
void Monitor_del(struct Monitor * monitor, struct cyclicBuffer * buffer);
// prints out counries/no accepted/no rejected to a log file
void m_log_file_print(struct Monitor * monitor);
