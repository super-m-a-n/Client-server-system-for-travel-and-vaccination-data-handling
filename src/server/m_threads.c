/* file : m_threads.c */
/* definitions and functions for the thread part of the monitorServer are developed here */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <assert.h>
#include <pthread.h>
#include "m_threads.h"
#include "m_helper.h"
#include "m_items.h"

#define perror_t(string, error) fprintf(stderr, "%s -> %s\n", string, strerror(error));		// an alternative to perror, that can be used with pthread functions

extern pthread_mutex_t mutex;			// mutex used to protect shared monitor server data among threads
extern pthread_cond_t not_empty;		// condition variable used to wait until cyclic buffer is not empty
extern pthread_cond_t not_full;		// condition variable used to wait until cyclic buffer is not full

struct cyclicBuffer * cyclicBuffer_init(int cyclicBufferSize)
{
	struct cyclicBuffer * buffer = malloc(sizeof(struct cyclicBuffer));
	if (buffer == NULL)
		fprintf(stderr, "[Error] : cyclicBuffer_init -> malloc \n\n");
	assert(buffer != NULL);

	buffer->size = cyclicBufferSize;				
	buffer->next_in = -1;				
	buffer->next_out = 0;				
	buffer->count = 0;					
	buffer->country_files = malloc(cyclicBufferSize * sizeof(char *));
	if (buffer->country_files == NULL)
		fprintf(stderr, "[Error] : cyclicBuffer_init -> malloc \n\n");
	assert(buffer->country_files != NULL);

	for (int i = 0; i < cyclicBufferSize; i++)
		buffer->country_files[i] = NULL;		// initialize all string to null (buffer is empty initially)

	return buffer;
}

void cyclicBuffer_del(struct cyclicBuffer * buffer)
{
	free(buffer->country_files);
	free(buffer);
}

int mutex_cond_init(pthread_mutex_t * mutex, pthread_cond_t * not_empty, pthread_cond_t * not_full)
{
	int err;

	if ((err = pthread_mutex_init(mutex, NULL)) != 0)		// initialize the mutex
	{
		perror_t("[Error] : mutex_cond_init -> pthread_mutex_init", err);
		return -1;
	}

	if ((err = pthread_cond_init(not_empty, NULL)) != 0)	// initialize the cond var not_empty
	{
		perror_t("[Error] : mutex_cond_init -> pthread_cond_init", err);
		return -1;
	}

	if ((err = pthread_cond_init(not_full, NULL)) != 0)		// initialize the cond var not_full
	{
		perror_t("[Error] : mutex_cond_init -> pthread_cond_init", err);
		return -1;
	}

	return 0;
}


void create_threads(struct Monitor * monitor, struct cyclicBuffer * buffer)
{
	int err;

	for (int i = 0; i < monitor->numThreads; i++)
	{
		// malloc a useful struct (thread info) that groups 2 pointers to heap data that the created thread must access 
		struct thread_info * info = malloc(sizeof(struct thread_info));
		if (info == NULL)
		{
			fprintf(stderr, "[Error] : create_threads -> malloc \n");
			exit(EXIT_FAILURE);
		}

		info->monitor = monitor;
		info->buffer = buffer;

		// create thread, with a pointer to the common cyclic buffer and a pointer to the common Monitor struct as arguments
		if ((err = pthread_create(&monitor->tids[i], NULL, consumer_thread, info) != 0))
		{
			perror_t("[Error] : create_threads -> pthread_create", err);
			exit(EXIT_FAILURE);
		}
	}
}

void * consumer_thread(void * info)
{
	struct Monitor * monitor = ((struct thread_info *) info)->monitor;			// save pointer to heap allocated monitor structure	(shared structure)
	struct cyclicBuffer * buffer = ((struct thread_info *) info)->buffer;		// save pointer to heap allocated cyclic buffer structure (shared structure)
	free(info); // dont leak the memory (info struct is no longer needed)

	int exiting = 0;

	while (!exiting)		// loop until main thread signals EXIT
	{
		pthread_mutex_lock(&mutex);		// lock the mutex
		while (buffer->count <= 0) 		// while the cyclic buffer is empty
		{
			pthread_cond_wait(&not_empty, &mutex);	// wait on the condition variable until buffer is not empty
		}

		char * country_file = buffer->country_files[buffer->next_out];		// get next country file from cyclic buffer

		if (!strcmp(country_file, "EXIT"))				// if main thread is signaling EXIT commands, exit after this iteration
			exiting = 1;
		else if (!strcmp(country_file, "DONE"))			// if main thread is signaling DONE, that means you are the thread that will send back the bloom filters
			send_bloom_filters(monitor);				// so send back the bloom filters to the travelMonitorClient
		else
		{
			if (read_subdir(monitor, country_file) < 0)		// read country file and update shared monitor structures
			{
				fprintf(stderr, "[Error] : consumer_thread -> read_subdir\n");
				pthread_exit(NULL);
			}
		}

		free(country_file);		// producer_thread has malloced it when inserting it into the buffer so free it
		country_file = NULL;

		buffer->next_out = (buffer->next_out + 1) % buffer->size;		// update next_out index
		buffer->count--;												// update buffer count
		pthread_cond_signal(&not_full);									// buffer definitely not full so signal any threads blocked on this condition variable
		pthread_mutex_unlock(&mutex);									// unlock the mutex
	}

	monitor = NULL;
	buffer = NULL;
	return NULL;		// thread exits
}

void producer_thread(struct Monitor * monitor, struct cyclicBuffer * buffer, const char ** paths)
{
	int i = 0;
	while (paths[i] != NULL)		// for each of the path1, path2, ..., pathn, NULL read subdirectory
	{
		char subdir[30];
		strncpy(subdir, paths[i], 30);
		if (write_subdir(monitor, buffer, subdir) < 0)		// write all country files of subdir into cyclic buffer
		{
			fprintf(stderr, "[Error] : producer_thread -> write_subdir\n");
			exit(EXIT_FAILURE);				// if an error occured all threads exit
		}

		i += 1;
	}

	if (write_done(monitor, buffer) < 0)		// after writing all country files, write a "DONE" string into buffer indicating end of country files
	{											// the thread that reads this DONE string will send back the bloom filters to the travelMonitorClient
		fprintf(stderr, "[Error] : producer_thread -> write_done\n");
		exit(EXIT_FAILURE);				// if an error occured all threads exit
	}

}

int read_subdir(struct Monitor * monitor, char * subdir)
{
	char * line = NULL;
	char * citizenID , * firstName, * lastName, * country, * virusName, * vacc, * date;
	int age;
    size_t length = 0;

    FILE *file_ptr;

	file_ptr = fopen(subdir, "r");  /*open citizen records txt file , in read mode*/
	if (file_ptr == NULL)
	{
		fprintf(stderr, "[Error] : read_subdir -> fopen, could not open file\n");
		return -1;
	}
	else    /*following block of code reads from the file and inserts the entries of file*/
	{
		// check if file is empty first
		int c = fgetc(file_ptr);	// read char from file
		if (c == EOF) // file is empty
		{
			fclose(file_ptr);
			return 0;	// ignore it and continue
		}

		ungetc(c, file_ptr);	// else undo char read
		while(getline(&line, &length, file_ptr) != -1)
		{
			date = NULL;
			line[strlen(line)-1] = '\0';		// remove newline character from line read from file
			char *str = strtok(line, " ");
			int i = 1;
			while(str != NULL)
			{
			    switch (i)
			    {
			        case 1: citizenID = str; break;
			        case 2: firstName = str; break;
			        case 3: lastName = str; break;
			        case 4: country = str; break;
			        case 5: age = atoi(str); break;
			        case 6: virusName = str; break;
			        case 7: vacc = str; break;
			        case 8: date = str; break;
			    }
			    
			    i++;
			    str = strtok(NULL, " ");
			}
			     
			Monitor_insert(monitor, citizenID, firstName, lastName, country, age, virusName, vacc, date);			     
		}

		free(line);
		fclose(file_ptr);
		return 0;
	}
}

int write_subdir(struct Monitor * monitor, struct cyclicBuffer * buffer, char * subdir)
{
	struct dirent ** file_list;
	int n;
	char * country_name;

	for (int i = (strlen(subdir) - 1); i >=0; i--)		// traverse the subdir backwards to find the last '/' to find the country name
	{
		if (subdir[i] == '/')
		{
			country_name = subdir + i + 1;
			break;
		}
	}

	if ( (n = scandir(subdir, &file_list, NULL, alphasort)) < 0)		// we use scandir to iterate over files in alphabetical order
	{
		fprintf(stderr, "[Error] : write_subdir -> scandir\n");
		return -1;
	}
	else
	{
		for (int i = 0; i < n; ++i)			// for each country file in subdirectory
		{
			if (!strcmp(file_list[i]->d_name, ".") || !strcmp(file_list[i]->d_name, ".."))	// if you are at . or .. just ignore
			{
				free(file_list[i]);
				continue;
			}
			else
			{
				pthread_mutex_lock(&mutex);		// lock the mutex
				M_CountryInfo country_info1 = (M_CountryInfo) hash_search(monitor->countries_info, country_name);
				if (country_info1 != NULL)	// if this country is already in countries HT
				{
					if (m_country_search_file(country_info1, file_list[i]->d_name) != NULL)		// and this file has already been read
					{
						free(file_list[i]);
						pthread_mutex_unlock(&mutex);									// unlock the mutex
						continue;			// just ignore it and continue
					}
				}
				else		// else this country is a new country
				{
					country_info1 = m_country_info_create(country_name);				// if given country name is new, create new country record
					hash_insert(monitor->countries_info, country_info1);			// insert it into countries index for future reference
				}

				char file_path[40];
	    		if (snprintf(file_path, 40, "%s/%s", subdir, file_list[i]->d_name) < 0)
	    		{
	    			fprintf(stderr, "[Error] : write_subdir -> snprintf\n");
	    			return -1;
	    		}

	    		while (buffer->count >= buffer->size)		// while cyclic buffer is full
	    		{
	    			pthread_cond_wait(&not_full, &mutex);	// wait until it is not full (wait on condition variable not_full)
	    		}

	    		buffer->next_in =  (buffer->next_in + 1) % buffer->size;		// update next_in index
	    		buffer->country_files[buffer->next_in] = malloc(strlen(file_path) + 1);
	    		if (buffer->country_files[buffer->next_in] == NULL)
	    		{
	    			fprintf(stderr, "[Error] : write_subdir -> malloc\n");
	    			return -1;
	    		}
	    		strcpy(buffer->country_files[buffer->next_in], file_path);		// put country file into shared cyclic buffer
	    		buffer->count++;				// update buffer count
				
				//M_CountryInfo country_info = (M_CountryInfo) hash_search(monitor->countries_info, country_name);
				//m_country_add_file(country_info, file_list[i]->d_name);
				m_country_add_file(country_info1, file_list[i]->d_name);		// add country file just read, into the list of read files for country
				free(file_list[i]);			// free memory used by scandir

				pthread_cond_signal(&not_empty);								// buffer definitely not empty so signal any threads blocked on this condition variable
				pthread_mutex_unlock(&mutex);									// unlock the mutex
			}
		}

		free(file_list);	// free memory used by scandir
		return 0;
	}	
}

int write_done(struct Monitor * monitor, struct cyclicBuffer * buffer)
{
	char * done_str = "DONE";
	pthread_mutex_lock(&mutex);		// lock the mutex
				
	while (buffer->count >= buffer->size)		// while cyclic buffer is full
	{
	    pthread_cond_wait(&not_full, &mutex);	// wait on the condition variable until it is not full
	}

	buffer->next_in =  (buffer->next_in + 1) % buffer->size;		// update next_in index
	buffer->country_files[buffer->next_in] = malloc(strlen(done_str) + 1);
	if (buffer->country_files[buffer->next_in] == NULL)
	{
	    fprintf(stderr, "[Error] : write_done -> malloc\n");
	    return -1;
	}
	strcpy(buffer->country_files[buffer->next_in], done_str);		// put DONE string into shared cyclic buffer
	buffer->count++;				// update buffer count
	
	pthread_cond_signal(&not_empty);								// buffer definitely not empty so signal any threads blocked on this condition variable
	pthread_mutex_unlock(&mutex);									// unlock the mutex

	return 0;
}

void destroy_threads(struct Monitor * monitor, struct cyclicBuffer * buffer)
{
	int err;

	if (write_exit(monitor, buffer) < 0)		// write EXIT string to shared cyclic buffer to indicate to the other threads it's time to exit
	{											
		fprintf(stderr, "[Error] : destroy_threads -> write_exit\n");
		exit(EXIT_FAILURE);				// if an error occured all threads exit
	}

	for (int i = 0; i < monitor->numThreads; ++i)
	{
		if ((err = pthread_join(monitor->tids[i], NULL)) != 0)
		{
			perror_t("[Error] : destroy_threads -> pthread_join", err);
			exit(EXIT_FAILURE);
		}
	}
}

int write_exit(struct Monitor * monitor, struct cyclicBuffer * buffer)
{

	for (int i = 0; i < monitor->numThreads; i++)		// for each thread created, write an EXIT string
	{
		char * exit_str = "EXIT";
		pthread_mutex_lock(&mutex);		// lock the mutex
					
		while (buffer->count >= buffer->size)		// while cyclic buffer is full
		{
		    pthread_cond_wait(&not_full, &mutex);	// wait on the condition variable until it is not full
		}

		buffer->next_in =  (buffer->next_in + 1) % buffer->size;		// update next_in index
		buffer->country_files[buffer->next_in] = malloc(strlen(exit_str) + 1);
		if (buffer->country_files[buffer->next_in] == NULL)
		{
		    fprintf(stderr, "[Error] : write_exit -> malloc\n");
		    return -1;
		}
		strcpy(buffer->country_files[buffer->next_in], exit_str);		// put EXIT string into shared cyclic buffer
		buffer->count++;				// update buffer count
		
		pthread_cond_signal(&not_empty);								// buffer definitely not empty so signal any threads blocked on this condition variable
		pthread_mutex_unlock(&mutex);									// unlock the mutex

	}

	return 0;
}

void mutex_cond_del(void)
{
	int err;

	if ((err = pthread_mutex_destroy(&mutex)) != 0)
	{
		perror_t("[Error] : mutex_cond_del -> pthread_mutex_destroy", err);
		exit(EXIT_FAILURE);
	}

	if ((err = pthread_cond_destroy(&not_empty)) != 0)
	{
		perror_t("[Error] : mutex_cond_del -> pthread_cond_destroy", err);
		exit(EXIT_FAILURE);
	}

	if ((err = pthread_cond_destroy(&not_full)) != 0)
	{
		perror_t("[Error] : mutex_cond_del -> pthread_cond_destroy", err);
		exit(EXIT_FAILURE);
	}
}
