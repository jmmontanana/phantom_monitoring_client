/*
 * Copyright 2014, 2015 High Performance Computing Center, Stuttgart
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#include "main.h" // variables like confFile
#include "thread_handler.h"
#include "plugin_manager.h" // functions like PluginManager_new(), PluginManager_free(), PluginManager_get_hook()
#include "plugin_discover.h" // variables like pluginCount, plugins_name; 
                             // functions like discover_plugins(), cleanup_plugins()
#include "mf_parser.h" // functions like mfp_parse(), ...
#include <excess_concurrent_queue.h> // functions like ECQ_create(), ECQ_free()

#define MIN_THREADS 1
#define BULK_SIZE 4
#define JSON_LEN 1024

#define SUCCESS 1
#define FAILURE 0
/*******************************************************************************
 * Variable Declarations
 ******************************************************************************/
int running;
static PluginManager *pm;
EXCESS_concurrent_queue_t data_queue;
pthread_t threads[256];
long timings[256];

/*******************************************************************************
 * Forward Declarations
 ******************************************************************************/
void catcher(int signo);
static void init_timings(void);
void *entryThreads(void *arg);  //threads entry for all threads
int checkConf(void);
int gatherMetric(int num);
int cleanSending(void);
static void init_timings(void);


/* All threads starts here */
int startThreads(void) {
	int t;
	running = 1;

	pm = PluginManager_new();
	const char *dirname = { "/plugins" };
	char *pluginLocation = malloc(256 * sizeof(char));
	strcpy(pluginLocation, pwd);
	strcat(pluginLocation, dirname);

	void* pdstate = discover_plugins(pluginLocation, pm);

	init_timings();

	/* calculate the sending threads number */
	//int sending_threads = (int) (pluginCount * publish_json_time * 1.0e9 / min_plugin_interval);
	int sending_threads = 1;
	int num_threads = MIN_THREADS + pluginCount + sending_threads;
	int iret[num_threads];
	int nums[num_threads];

	/* create the data metrics queue */
	data_queue = ECQ_create(0);

	for (t = 0; t < num_threads; t++) {
		nums[t] = t;
		iret[t] = pthread_create(&threads[t], NULL, entryThreads, &nums[t]);
		if (iret[t]) {
			printf("ERROR: return code from pthread_create() is %d.\n", iret[t]);
			exit(FAILURE);
		}
	}

	struct sigaction sig;
	sig.sa_handler = catcher;
	sig.sa_flags = SA_RESTART;
	sigemptyset(&sig.sa_mask);
	sigaction(SIGTERM, &sig, NULL );
	sigaction(SIGINT, &sig, NULL );

	while (running)
		sleep(1);
	
	//thread join from plugins threads till all the sending threads
	for (t = MIN_THREADS; t < num_threads; t++) {
		pthread_join(threads[t], NULL);
	}

	cleanup_plugins(pdstate);
	//shutdown_curl();
	PluginManager_free(pm);
	ECQ_free(data_queue);
	free(pluginLocation);
	return 1;
}

/* entry for all threads */
void* entryThreads(void *arg) {
	int *typeT = (int*) arg;
	if(*typeT == 0) {
		checkConf();
	}
	else if((MIN_THREADS <= *typeT) && (*typeT < MIN_THREADS + pluginCount)) {
		gatherMetric(*typeT);
	}
	else {
		cleanSending();
	}
	return NULL;
}

/* timings update if mf_config.ini has been modified */
int checkConf(void) 
{
	while (running) {
		mfp_parse(confFile);
		char wait_some_seconds[20] = {'\0'};
		mfp_get_value("timings", "update_configuration", wait_some_seconds);
		sleep(atoi(wait_some_seconds));
		init_timings();
	}
	return SUCCESS;
}

/* each plugin gathers its metrics at a specific rate and send the json-formatted metrics to mf_server */
int gatherMetric(int num) 
{
	int i;
	char* current_plugin_name = plugins_name[num - MIN_THREADS];

	struct timespec tim = { 0, 0 };
	struct timespec tim2;

	if (timings[num - MIN_THREADS] >= 10e8) {
		tim.tv_sec = timings[num - MIN_THREADS] / 10e8;
		tim.tv_nsec = timings[num- MIN_THREADS] % (long) 10e8;
	} else {
		tim.tv_sec = 0;
		tim.tv_nsec = timings[num - MIN_THREADS];
	}
	PluginHook hook = PluginManager_get_hook(pm);
	printf("gather metric %s (#%d) with update interval of %ld ns\n", current_plugin_name, (num- MIN_THREADS), timings[num - MIN_THREADS]);

	EXCESS_concurrent_queue_handle_t data_queue_handle;
	data_queue_handle =ECQ_get_handle(data_queue);

	char json_array[JSON_LEN * BULK_SIZE] = {'\0'};
	json_array[0] = '[';
	char msg[JSON_LEN] = {'\0'};
	char static_json[512] = {'\0'};

	sprintf(static_json, "{\"application_id\":\"%s\",\"experiment_id\":\"%s\",\"component_id\":\"%s\",", 
		application_id, experiment_id, component_id);

	while (running) {

		for(i=0; i<BULK_SIZE; i++) {
			memset(msg, '\0', JSON_LEN * sizeof(char));
			char *json = hook();	//malloc of json in hook()
			nanosleep(&tim, &tim2);
			sprintf(msg, "%s%s},", static_json, json);
			strcat(json_array, msg);
			if(json != NULL) {
				free(json);
			}
		}
		json_array[strlen(json_array) -1] = ']';
		json_array[strlen(json_array)] = '\0';
		puts(json_array);
		memset(json_array, '\0', JSON_LEN * BULK_SIZE * sizeof(char));
		json_array[0] = '[';
		//ECQ_enqueue(data_queue_handle, m_ptr);
	}
	/* call when terminating program, enables cleanup of plug-ins */
	ECQ_free_handle(data_queue_handle);
	
	hook();
	return SUCCESS;
}

int cleanSending(void) 
{
	return SUCCESS;
}

/* catch the stop signal */
void catcher(int signo) 
{
	running = 0;
	printf("Signal %d catched.\n", signo);
}

/* parse mf_cconfig.ini to get all timing information */
static void init_timings(void)
{
	char *ptr;
	char timing[20] = {'\0'};
	mfp_get_value("timings", "default", timing);
	long default_timing = strtol(timing, &ptr, 10);

	for (int i = 0; i < pluginCount; ++i) {
		if (plugins_name[i] == NULL) {
			continue;
		}
		char value[20] = {'\0'};
		char *ptr;
		mfp_get_value("timings", plugins_name[i], value);
		if (value[0] == '\0') {
			timings[i] = default_timing;
		} else {
			timings[i] = strtol(value, &ptr, 10);
			printf("timing for plugin %s is %ldns\n", plugins_name[i], timings[i]);
		}
	}
}