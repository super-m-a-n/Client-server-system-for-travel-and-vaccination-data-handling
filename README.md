# Systems Programming Project 3

## About
This project implements a client-servers system which accepts / rejects various user travel requests and handles queries based on their vaccination records. We have one ```travelMonitorClient``` client process which uses sockets to communicate with a number of ```travelMonitorServer```  server processes using a self-designed communication protocol. The vaccination records / data are read using a number of threads created by the server processes. Sample vaccination data can be found in folder ```/input_dir```. Detailed information about the project's specifications and the user requests on the data can be found in the project's pdf file : ```hw2-spring-2021.pdf```.
