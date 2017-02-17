CC = gcc

CFLAGS = -std=gnu99 -pedantic -Wall -Wwrite-strings -Wpointer-arith \
-Wcast-align -O0 -ggdb -I../src $(CORE_INC) $(CURL_INC) $(PUBLISH_INC) 

LFLAGS = -lm -lpthread $(PUBLISH) $(CURL)

COMMON = ${CURDIR}/../..

PUBLISH_INC = -I${COMMON}/publisher/src
CORE_INC = -I$(COMMON)/core
CURL_INC = -I$(COMMON)/../bin/curl/include

PUBLISH = -L${COMMON}/publisher -lpublisher
CURL = -L$(COMMON)/../bin/curl/lib -lcurl

SRC = ../src

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CFLAGS += -DDEBUG -g
endif

all: test_mf_api

test_mf_api: test_mf_api.c
	$(CC) -o $@ $^ ${SRC}/*.c $(CFLAGS) $(LFLAGS)

clean:
	rm -rf test_mf_api