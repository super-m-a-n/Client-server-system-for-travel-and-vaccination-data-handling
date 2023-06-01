/* file : m_helper.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <pthread.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "m_helper.h"
#include "skip_list.h"
#include "bloom.h"
#include "hash.h"
#include "list.h"
#include "m_items.h"
#include "messages.h"
#include "m_threads.h"

extern pthread_mutex_t mutex;			// mutex used to protect shared monitor server data among threads

/*===================== INITIALIZATION PHASE ===========================*/

struct Monitor * Monitor_init(int bufferSize, int numThreads, unsigned int bloom_size, int max_level, float p)
{
	struct Monitor * monitor = malloc(sizeof(struct Monitor));
	if (monitor == NULL)
		fprintf(stderr, "Error : Monitor_init -> malloc \n\n");
	assert(monitor != NULL);

	monitor->citizens_info = hash_create(100, 0);
	monitor->viruses_info = hash_create(10, 1);
	monitor->countries_info = hash_create(10, 2);
	monitor->bufferSize = bufferSize;
	monitor->bloom_size = bloom_size;
	monitor->max_level = max_level;
	monitor->p = p;
	monitor->numThreads = numThreads;
	monitor->accepted = 0;
	monitor->rejected = 0;
	monitor->tids = malloc(numThreads * sizeof(pthread_t));
	if (monitor->tids == NULL)
		fprintf(stderr, "Error : Monitor_init -> malloc \n\n");
	assert(monitor->tids != NULL);

	bloomSize_init(bloom_size);		// initialize bloomSize for messages.c

	return monitor;
}

int setup_server(in_port_t port)
{
	int socket_fd, final_socket_fd;
	int reuse_addr = 1; 	// used so we can bind again in future execution, regardless if previous connection is still in TIME_WAIT state */
	if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{																									
		perror("[Error] : setup_server -> socket\n");
		return -1;
	}

	if (setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0)		// set SO_REUSEADDR flag so we can re-bind in future execution
	{
		perror("[Error] : setup_server -> setsockopt\n");
		return -1;
	}

	char host_name[100];
	struct sockaddr_in sock_addr;
	struct hostent *h;

	sock_addr.sin_family = AF_INET;				// construct the socket address
	sock_addr.sin_port = htons(port);
	if (gethostname(host_name, sizeof(host_name)) < 0)	// get host name, so that we can get the IP using gethostbyname
	{
		fprintf(stderr, "[Error] : setup_server -> gethostname\n");
		return -1;		
	}

	if ((h = gethostbyname(host_name)) == NULL)			// get the hostent struct using gethostbyname
	{
		if (h_errno == HOST_NOT_FOUND)
			fprintf(stderr, "[Error] : setup_server -> gethostbyname : HOST_NOT_FOUND\n");
		else
			fprintf(stderr, "[Error] : setup_server -> gethostbyname\n");
		return -1;
	}

	//printf("IP address (server) : %s\n", inet_ntoa(*((struct in_addr *)h->h_addr_list[0])));				// extract the first IP address in the form with dots
	sock_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)h->h_addr_list[0])));	// convert it to a network IP number and pass it to the socket address struct

	if (bind(socket_fd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0)
	{
		perror("[Error] : setup_server -> bind\n");
		return -1;
	}

	if (listen(socket_fd, SOMAXCONN) < 0)
	{
		perror("[Error] : setup_server -> listen\n");
		return -1;
	}

	if ((final_socket_fd = accept(socket_fd, NULL, 0)) < 0)
	{
		perror("[Error] : setup_server -> accept\n");
		return -1;
	}

	close(socket_fd);
	return final_socket_fd;
}

void send_bloom_filters(struct Monitor * monitor)
{
	// you have read all of clients subdirs-paths, so send him back the bloom filters
	M_VirusInfo virus_info;
	// iterate upon the hash-table of viruses
	while ((virus_info = hash_iterate_next(monitor->viruses_info)) != NULL)
	{
		void * response_msg = create_msg2(m_get_virus_name(virus_info), monitor->bloom_size , m_get_bloom_filter(virus_info));	// create message
		send_message(monitor->socket_fd, MSG2, response_msg, monitor->bufferSize);				// send message
	}

	/* when you are done with sending the bloom filters, notify client that you are done and ready for other commands */
	send_message(monitor->socket_fd, DONE, NULL, monitor->bufferSize);
}


void Monitor_insert(struct Monitor * monitor, char * citizenID , char * firstName, char * lastName, char * country, unsigned int age, char * virusName, char * vacc, char * date)
{

	// search for an already existing citizen record with same ID
	M_CitizenInfo citizen_info = (M_CitizenInfo) hash_search(monitor->citizens_info, citizenID);
	// search for an already existing virus record with given name
	M_VirusInfo virus_info = (M_VirusInfo) hash_search(monitor->viruses_info, virusName);
	// search for an already existing country record with given name
	M_CountryInfo country_info = (M_CountryInfo) hash_search(monitor->countries_info, country);

	if (citizen_info != NULL)		// if a citizen record with same ID already exists
	{
		// check if new record is inconsistent
		if (strcmp(firstName, m_get_citizen_name(citizen_info)) != 0 || strcmp(lastName, m_get_citizen_surname(citizen_info)) != 0 
			|| strcmp(country, m_get_citizen_country(citizen_info)) != 0 || age != m_get_citizen_age(citizen_info))
		{
			printf("ERROR IN RECORD : %s %s %s %s %d %s %s ", citizenID, firstName, lastName, country, age, virusName, vacc); printf( (date == NULL) ? "\n" : "%s\n", date);
			printf("INCONSISTENT INPUT DATA\n\n");
			return;
		}

		if (virus_info != NULL)
		{
			char * temp_date;
			// check if new record is duplicate (same ID, but also same virus - that means, an entry with given ID already exists for given virus)
			// if it exists it is either on the vaccinated skip list or non vaccinated skip list for given virus
			if (skip_list_search(m_get_vacc_list(virus_info), citizenID, &temp_date) || skip_list_search(m_get_non_vacc_list(virus_info), citizenID, &temp_date))
			{
				printf("ERROR IN RECORD : %s %s %s %s %d %s %s ", citizenID, firstName, lastName, country, age, virusName, vacc); printf( (date == NULL) ? "\n" : "%s\n", date);
				printf("INPUT DATA DUPLICATION\n\n");
				return;
			}
		}
	}

	// at last, check for invalid data form, i.e. vaccinated == "YES" but no date is given or vaccinated = "NO" but a date is given
	if ( ( !strcmp(vacc, "YES") && date == NULL) || (!strcmp(vacc, "NO") && date != NULL) )
	{
		printf("ERROR IN RECORD : %s %s %s %s %d %s %s ", citizenID, firstName, lastName, country, age, virusName, vacc); printf( (date == NULL) ? "\n" : "%s\n", date);
		printf("INVALID INPUT DATA FORM\n\n");
		return;
	}

	if (country_info == NULL)
	{
		country_info = m_country_info_create(country);		// if given country name is new, create new country record
		hash_insert(monitor->countries_info, country_info);			// insert it into countries index for future reference
	}

	if (citizen_info == NULL)			// given record is a new citizen record (new ID)
	{
		citizen_info = m_citizen_info_create(citizenID, firstName, lastName, age, country_info);	// create new citizen record
		hash_insert(monitor->citizens_info, citizen_info);				// insert it into citizens index for future reference
	}

	if (virus_info == NULL)
	{
		virus_info = m_virus_info_create(virusName, monitor->bloom_size, monitor->max_level, monitor->p);
		hash_insert(monitor->viruses_info, virus_info);
	}

	// insert citizen into bloom filter, correct skip list, of given virus
	if (!strcmp(vacc, "YES"))
	{
		bloom_insert(m_get_bloom_filter(virus_info), (unsigned char*) citizenID);	// bloom filter of virus, keeps track of the vaccinated citizens
		skip_list_insert(m_get_vacc_list(virus_info), citizen_info, date);		// insert into vaccinated persons skip list if citizen was vaccinated
	}
	else
		skip_list_insert(m_get_non_vacc_list(virus_info), citizen_info, date);	// insert into not vaccinated skip list if citizen was not vaccinated
	
}


/* =================== QUERY PHASE ========================= */

/* wrapper function that calls a specific function to take action based on message received */
int Monitor_take_action(struct Monitor * monitor, struct cyclicBuffer * buffer, int msgd, void * message, char * subdir)
{
	if (msgd != EXIT && msgd != MSG1 && msgd != MSG3 && msgd != MSG5 && msgd != MSG8)		// MonitorServer handles message descriptors that refer to him only
		return -1;
	if (msgd == MSG1)
	{
		memset(subdir, '\0', strlen(subdir));
		if (decode_msg1(msgd, message, subdir) < 0)					// decode message of type MSG1
			return -1;
		// subdir is now initialized after decoding message
		char * paths[2];
		paths[0] = subdir;
		paths[1] = NULL;
		producer_thread(monitor, buffer, (const char **) &paths[0]);						// call producer thread to put the new country files into the shared cyclic buffer and have the other threads read them
		//send_message(monitor->socket_fd, DONE, NULL, monitor->bufferSize);	// notify parent you are done
	}
	else if (msgd == MSG3)
	{
		char citizenID[6], virus[20];
		if (decode_msg3(msgd, message, citizenID, virus) < 0)		// decode message of type MSG3
			return -1;
		pthread_mutex_lock(&mutex);										// lock the mutex (vaccineStatus accesses shared structures)
		vaccineStatus(monitor, citizenID, virus);
		pthread_mutex_unlock(&mutex);									// unlock the mutex
	}
	else if (msgd == MSG5)
	{
		char citizenID[6];
		if (decode_msg5(msgd, message, citizenID) < 0)				// decode message of type MSG5
			return -1;
		pthread_mutex_lock(&mutex);										// lock the mutex (vaccineStatus accesses shared structures)
		vaccineStatus(monitor, citizenID, NULL);
		pthread_mutex_unlock(&mutex);									// unlock the mutex
	}
	else if (msgd == MSG8)
	{
		int result;
		if (decode_msg8(msgd, message, &result) < 0)
			return -1;
		if (result == 1)
			monitor->accepted += 1;
		else
			monitor->rejected += 1;

		send_message(monitor->socket_fd, DONE, NULL, monitor->bufferSize);	// notify client you are done and move on to other commands
	}
	else if (msgd == EXIT)
		exit_monitorServer(monitor, buffer);

	delete_message(message);  // message no longer needed
	return 0;	
}

void vaccineStatus(struct Monitor * monitor, char * citizenID, char * virusName)
{
	// search for an existing cititzen record with given citizen ID
	M_CitizenInfo citizen_info = (M_CitizenInfo) hash_search(monitor->citizens_info, citizenID);
	if (citizen_info == NULL && virusName != NULL)
	{
		fprintf(stderr, "Error : Monitor -> vaccineStatus -> Given citizen ID does not exist in Monitor's database\n\n");
		return;
	}

	if (virusName != NULL)		// if specific virusName was given (i.e. query /travelRequest )
	{
		// search for an existing virus record with given virus name
		M_VirusInfo virus_info = (M_VirusInfo) hash_search(monitor->viruses_info, virusName);
		if (virus_info == NULL)
		{
			fprintf(stderr, "Error : Monitor -> vaccineStatus -> Given virus name does not exist in Monitors's database\n\n");
			return;
		}

		char * date = NULL;
		void * message;

		if (!skip_list_search(m_get_vacc_list(virus_info), citizenID, &date))		// if citizen id was not found into vaccinated skip list for given virus
		{
			message = create_msg4("NO", date);		// construct message
			//monitor->rejected += 1;
		}
		else
		{
			message = create_msg4("YES", date);		// construct message
			//monitor->accepted += 1;
		}

		send_message(monitor->socket_fd, MSG4, message, monitor->bufferSize);	// send message
	}
	else		// no specific virusName was given (i.e. query /searchVaccinationStatus )
	{
		if (citizen_info != NULL)	// if no info found for given citizenID, we just send back DONE immediately
		{
			// create message of type MSG6
			void * message = create_msg6(m_get_citizen_name(citizen_info), m_get_citizen_surname(citizen_info), m_get_citizen_country(citizen_info), m_get_citizen_age(citizen_info));
			send_message(monitor->socket_fd, MSG6, message, monitor->bufferSize);  // to start things off, send back name,surname,country,age about given citizenID 
			// and then send back all vaccination info you can find for given citizenID
			M_VirusInfo virus_info;
			// iterate upon the hash-table of viruses
			while ((virus_info = hash_iterate_next(monitor->viruses_info)) != NULL)
			{
				char * date = NULL;
				if (skip_list_search(m_get_vacc_list(virus_info), citizenID, &date))		// if citizen id was found into vaccinated skip list for given virus
				{
					void * message = create_msg7(m_get_virus_name(virus_info), "YES", date);	// create message of type MSG7
					send_message(monitor->socket_fd, MSG7, message, monitor->bufferSize);
				}
				else if (skip_list_search(m_get_non_vacc_list(virus_info), citizenID, &date)) // if citizen id was found into not vaccinated list for given virus
				{
					void * message = create_msg7(m_get_virus_name(virus_info), "NO", date);
					send_message(monitor->socket_fd, MSG7, message, monitor->bufferSize);
				}
				// if citizen is not associated with particular virus, then we dont send back anything
			}
			
		}
		/* when you are done with sending vaccination info, notify parent that you are done and ready for other commands */
		send_message(monitor->socket_fd, DONE, NULL, monitor->bufferSize);
	}
}




/*==================== EXIT PHASE ========================== */

void exit_monitorServer(struct Monitor * monitor, struct cyclicBuffer * buffer)
{
	pthread_mutex_lock(&mutex);		// lock the mutex (m_log_file_print accesses shared structures)
	m_log_file_print(monitor);
	pthread_mutex_unlock(&mutex);	// unlock the mutex
	Monitor_del(monitor, buffer);
	exit(EXIT_SUCCESS);
}

void Monitor_del(struct Monitor * monitor, struct cyclicBuffer * buffer)
{
	destroy_threads(monitor, buffer);			// join created threads and do thread related cleanup
	hash_destroy(monitor->countries_info);
	hash_destroy(monitor->citizens_info);
	hash_destroy(monitor->viruses_info);
	close(monitor->socket_fd);
	free(monitor->tids);
	mutex_cond_del();							// destroy mutex, cond variables
	cyclicBuffer_del(buffer);					// delete the cyclic buffer struct
	free(monitor);
}

void m_log_file_print(struct Monitor * monitor)
{
	FILE * file_ptr;
	char log_file_name[40];
	snprintf(log_file_name, 40, "log_file.%d.txt", getpid());	// create log file name
	file_ptr = fopen(log_file_name, "w");						// open file for writing (if exists already we just overwrite it)
	if (file_ptr == NULL)						
	{
		fprintf(stderr, "[Error] : Monitor -> log_file_print -> Could not open/create log file\n\n");
		return;
	}

	M_CountryInfo country_info;
	// iterate upon the hash-table of countries of Monitor
	while ((country_info = (M_CountryInfo) hash_iterate_next(monitor->countries_info)) != NULL)
		fprintf(file_ptr, "%s\n", m_get_country_name(country_info));				// print all countries that participated in travelMonitor
	fprintf(file_ptr, "TOTAL TRAVEL REQUESTS %d\n", monitor->accepted + monitor->rejected);		// print total travel requests
	fprintf(file_ptr, "ACCEPTED %d\n", monitor->accepted);								// print the #accepted
	fprintf(file_ptr, "REJECTED %d\n", monitor->rejected);								// print the #rejected
	fclose(file_ptr);
}