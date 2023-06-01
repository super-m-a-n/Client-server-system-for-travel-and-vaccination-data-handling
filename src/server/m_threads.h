/* file : m_threads.h */
/* definitions and functions for the thread part of the monitorServer are developed here */
#pragma once
#include <pthread.h>
#include "m_helper.h"

struct cyclicBuffer			// a struct for the cyclicBuffer that keeps track of all variables we care about
{						
	int size;					// the cyclic buffer size
	int next_in;				// the index of the buffer where the next item will be placed
	int next_out;				// the index of the buffer where the next item will be removed from
	int count;					// number of elements currently in buffer
	char ** country_files;		// an array of strings/country-files
};

struct thread_info				// useful struct that groups 2 pointers to heap data that the created thread must access 
{
	struct Monitor * monitor;
	struct cyclicBuffer * buffer;
};

/*__________________________________________________________________________*/

/* creates the the cyclicBuffer structure and initializes all its components needed */
struct cyclicBuffer * cyclicBuffer_init(int cyclicBufferSize);
/* destroys the cyclicBuffer structure and all of its substructures that were used*/
void cyclicBuffer_del(struct cyclicBuffer * buffer);

/* initializes the given mutex and the given condition variables */
int mutex_cond_init(pthread_mutex_t * mutex, pthread_cond_t * not_empty, pthread_cond_t * not_full);
/* destroy mutex, cond variables */
void mutex_cond_del(void);

/* create the helper threads and assign them to read the input country files that the monitor server puts in cyclic buffer */
void create_threads(struct Monitor * monitor, struct cyclicBuffer * buffer);
/* join created threads and do thread related cleanup */
void destroy_threads(struct Monitor * monitor, struct cyclicBuffer * buffer);

/* starting function for all created threads (consumers) */
void * consumer_thread(void * info);
/* consumer threads reads the subdirectory indicated by char * subdir and updates structures */
int read_subdir(struct Monitor * monitor, char * subdir);

/* function for the main calling thread, which is the producer thread */
/* places all the country files in the directories indicated by paths, into the cyclic buffer */
/* after it has placed all the country files, it signals one of the threads to send back the bloom filters to the client */
void producer_thread(struct Monitor * monitor, struct cyclicBuffer * buffer, const char ** paths);
/* main producer thread writes subdirectory path (subdir) into the shared cyclic buffer */
int write_subdir(struct Monitor * monitor, struct cyclicBuffer * buffer, char * subdir);
/* main producer thread writes DONE string into the shared cyclic buffer to indicate end of file input*/
int write_done(struct Monitor * monitor, struct cyclicBuffer * buffer);
/* main producer thread writes EXIT string into the shared cyclic buffer to indicate exit*/
int write_exit(struct Monitor * monitor, struct cyclicBuffer * buffer);