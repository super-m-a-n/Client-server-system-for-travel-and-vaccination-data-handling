SRC = ./src
OBJS = ./obj
STRUCTS = $(SRC)/structs
CLIENT = $(SRC)/client
SERVER = $(SRC)/server
UTILS = $(SRC)/utils

CC = gcc
CFLAGS = -g -Wall -I. -I$(STRUCTS) -I$(UTILS) -I$(CLIENT) -I$(SERVER)
target: travelMonitorClient monitorServer

OBJS_CLIENT = travelMonitorClient.o input_check_client.o tm_helper.o
OBJS_SERVER = monitorServer.o m_helper.o m_threads.o input_check_server.o 
OBJS_COMMON = date.o messages.o bloom.o skip_list.o list.o hash.o m_items.o tm_items.o


bloom.o: $(STRUCTS)/bloom.c
	$(CC) $(CFLAGS) -c $(STRUCTS)/bloom.c
skip_list.o: $(STRUCTS)/skip_list.c
	$(CC) $(CFLAGS) -c $(STRUCTS)/skip_list.c
m_items.o: $(SERVER)/m_items.c
	$(CC) $(CFLAGS) -c $(SERVER)/m_items.c
tm_items.o: $(CLIENT)/tm_items.c
	$(CC) $(CFLAGS) -c $(CLIENT)/tm_items.c	
list.o: $(STRUCTS)/list.c
	$(CC) $(CFLAGS) -c $(STRUCTS)/list.c
hash.o: $(STRUCTS)/hash.c
	$(CC) $(CFLAGS) -c $(STRUCTS)/hash.c
input_check_client.o: $(CLIENT)/input_check_client.c
	$(CC) $(CFLAGS) -c $(CLIENT)/input_check_client.c
input_check_server.o: $(SERVER)/input_check_server.c
	$(CC) $(CFLAGS) -c $(SERVER)/input_check_server.c
date.o: $(UTILS)/date.c
	$(CC) $(CFLAGS) -c $(UTILS)/date.c
messages.o: $(UTILS)/messages.c
	$(CC) $(CFLAGS) -c $(UTILS)/messages.c
m_helper.o: $(SERVER)/m_helper.c
	$(CC) $(CFLAGS) -c $(SERVER)/m_helper.c
tm_helper.o: $(CLIENT)/tm_helper.c
	$(CC) $(CFLAGS) -c $(CLIENT)/tm_helper.c
m_threads.o: $(SERVER)/m_threads.c
	$(CC) $(CFLAGS) -c $(SERVER)/m_threads.c -lpthread
travelMonitorClient.o: $(SRC)/travelMonitorClient.c
	$(CC) $(CFLAGS) -c $(SRC)/travelMonitorClient.c
monitorServer.o: $(SRC)/monitorServer.c
	$(CC) $(CFLAGS) -c $(SRC)/monitorServer.c

travelMonitorClient: $(OBJS_COMMON) $(OBJS_CLIENT)
	$(CC) $(CFLAGS) $(OBJS_COMMON) $(OBJS_CLIENT) -o travelMonitorClient 
	mkdir -p $(OBJS)
	mv -f $(OBJS_COMMON) $(OBJS)
	mv -f $(OBJS_CLIENT) $(OBJS)
monitorServer: $(OBJS_COMMON) $(OBJS_SERVER)
	$(CC) $(CFLAGS) $(OBJS_COMMON) $(OBJS_SERVER) -o monitorServer -lpthread
	mv -f $(OBJS_COMMON) $(OBJS)
	mv -f $(OBJS_SERVER) $(OBJS)

.PHONY: clean

clean:
	rm -f travelMonitorClient
	rm -f monitorServer
	rm -rf $(OBJS)
