/*file : travelMonitorClient.c*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <dirent.h>
#include "input_check_client.h"
#include "tm_helper.h"


int main(int argc, char const *argv[])
{
	int numMonitors, socketBufferSize, cyclicBufferSize, numThreads;
	unsigned int bloom_size;
	DIR * input_dir;
	
	/* check for correct arg input from terminal and initialize program parameters */
	if (!check_init_args_client(argc, argv, &numMonitors, &socketBufferSize, &cyclicBufferSize, &bloom_size, &input_dir, &numThreads))
		exit(EXIT_FAILURE);

	/* initialization phase (part1) */
	// initialize structures kept by travelMonitorClient
	struct travelMonitor * travelMonitor = travelMonitor_init(numMonitors, socketBufferSize, bloom_size, input_dir);
	// fork monitorServers, execp on them, after assigning subdirectories to them and setup client-side of sockets
	ipc_init(travelMonitor, input_dir, argv[10], argv[12], argv[4], argv[6], argv[8]);

	/* initialization phase (part2) */
	wait_monitors_bfs(travelMonitor);						// wait on monitor Servers to return bloom filters etc. via select

	/* executing queries/commands phase */
	bool exiting = false;
	while (exiting == false)
	{
		char input[100];
		printf("Waiting for command/task >>  ");

		fgets(input, 100, stdin);

		if (!strcmp(input, "\n"))
			continue;
		input[strlen(input)-1] = '\0';									// remove newline character from line read from command line

		exiting = check_cmd_args(travelMonitor, input, argv[10]);		// check if cmd line input was correct and take necessary actions if so
	}
	/* exiting now */
	exit(EXIT_SUCCESS);
}