/* file : input_check_server.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <netinet/in.h>
#include "input_check_server.h"


bool check_init_args_server(int argc, const char ** argv, in_port_t * port, int * numThreads, int * socketBufferSize, int * cyclicBufferSize, unsigned int * bloom_size)
{
	if (argc <= 11)
	{
		fprintf(stderr, "Error : wrong number of args - missing args\n");
		return false;
	}

	if (strcmp(argv[1], "-p") != 0 || strcmp(argv[3], "-t") != 0 || strcmp(argv[5], "-b") != 0 || strcmp(argv[7], "-c") != 0 || strcmp(argv[9], "-s") != 0)
	{
		fprintf(stderr, "Error: one or more wrong input parameters\n Use : -p -t -b -c -s\n");
		return false;
	}

	// check if port is indeed a positive integer
	if (!is_integer(argv[2]) || !atoi(argv[2]))
	{
		fprintf(stderr, "Error: invalid input parameter port\n");
		return false;
	}
	
	*port = atoi(argv[2]);

	// check if numThreads is indeed a positive integer
	if (!is_integer(argv[4]) || !atoi(argv[4]))
	{
		fprintf(stderr, "Error: invalid input parameter numThreads\n");
		return false;
	}

	*numThreads = atoi(argv[4]);

	// check if socketBufferSize is indeed a positive integer and at least 4 bytes
	if (!is_integer(argv[6]) || !atoi(argv[6]) || atoi(argv[6]) < sizeof(int))
	{
		fprintf(stderr, "Error: invalid input parameter socketBufferSize\n");
		return false;
	}

	*socketBufferSize = atoi(argv[6]);

	// check if cyclicBufferSize is indeed a positive integer
	if (!is_integer(argv[8]) || !atoi(argv[8]) )
	{
		fprintf(stderr, "Error: invalid input parameter cyclicBufferSize\n");
		return false;
	}

	*cyclicBufferSize = atoi(argv[8]);

	// check if bloom_size is indeed a positive integer
	if (!is_integer(argv[10]) || !atoi(argv[10]))
	{
		fprintf(stderr, "Error: invalid input parameter bloom_size\n");
		return false;
	}

	*bloom_size = atoi(argv[10]);

	return true;	// everything goes fine

}

bool is_integer(const char * string)
{
	for (int i = 0; i < strlen(string); i++)
	{
		if (string[i] < '0' || string[i] > '9')
			return false;
	}

	return true;
}