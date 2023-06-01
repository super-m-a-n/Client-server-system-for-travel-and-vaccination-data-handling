/* file : input_check_server.h */
#pragma once
#include <stdbool.h>
#include <dirent.h>
#include <netinet/in.h>

/* checks for correct input args from execv from client and initializes program parameters if so */
bool check_init_args_server(int argc, const char ** argv, in_port_t * port, int * numThreads, int * socketBufferSize, int * cyclicBufferSize, unsigned int * bloom_size);
/* checks if given string, is a string of just numbers (integer) */
bool is_integer(const char * string);