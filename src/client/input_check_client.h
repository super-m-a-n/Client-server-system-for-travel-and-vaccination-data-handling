/* file : input_check_client.h */
#pragma once
#include <stdbool.h>
#include <dirent.h>
#include <netinet/in.h>
#include "tm_helper.h"

/* checks for correct input args from terminal and initializes program parameters if so */
bool check_init_args_client(int argc, const char ** argv, int * numMonitors, int * socketBufferSize, int * cyclicBufferSize, unsigned int * bloom_size, DIR ** dir, int * numThreads);
/* checks if given string, is a string of just numbers (integer) */
bool is_integer(const char * string);
/* checks if given input string, corresponds to a valid query, and if so, takes the necessary actions to answer to that query */
/* returns true if command /exit was given otherwise returns false */
bool check_cmd_args(struct travelMonitor * travelMonitor, char * input, const char * input_dir_name);