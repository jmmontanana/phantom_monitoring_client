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

#include <ctype.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>

#include "main.h"
#include "mf_debug.h"		//functions like log_error(), log_info()...
#include "thread_handler.h"	//functions like startThreads()...
#include "mf_parser.h" 		//functions like mfp_parse()...

#define DEFAULT_APP "monitoring_test"
#define DEFAULT_EXP "12345678"
#define DEFAULT_COMP "comp_1"

#define SUCCESS 1
#define FAILURE 0

/*******************************************************************************
 * External Variables Declarations
 ******************************************************************************/
char *pwd;
char *confFile;
char *application_id;
char *experiment_id;
char *component_id;

char *metrics_publish_URL;
char *platform_id;

FILE *logFile;		//declared in mf_debug.h

/*******************************************************************************
 * Variable Declarations
 ******************************************************************************/
char *name;
int pwd_is_set = 0;

/*******************************************************************************
 * Forward Declarations
 ******************************************************************************/
void set_pwd(void);
void createLogFile(void);
int writeTmpPID(void);
int prepare(void);


/* everything starts here */
int main(int argc, char* argv[]) {
	extern char *optarg;
	int c;
	int application_flag = 0; //arg "application_id" exists flag
	int experiment_flag = 0;	//arg "experiment_id" exists flag
	int component_flag = 0;	//arg "component_id" exists flag

	int err = 0, help = 0;
	
	/* init arguments */
	pwd = calloc(256, sizeof(char));
	name = calloc(256, sizeof(char)); //name of the tmp pid file
	application_id = calloc(128, sizeof(char));
	experiment_id = calloc(128, sizeof(char));
	component_id = calloc(128, sizeof(char));

	confFile = calloc(256, sizeof(char));

	/* assigns the current working directory to a variable */
	set_pwd();

	/* create log file, use log_error(), log_info() and log_warn() afterwards */
	createLogFile();

	/* write PID to a file ("tmp_pid") in `pwd`; can be used to kill the agent */
	if(writeTmpPID() == FAILURE) {
		exit(FAILURE);
	}

	/* parse command-line arguments */
	while ((c = getopt(argc, argv, "a:e:c:h")) != -1)
		switch (c) {
		case 'a':
			strcpy(application_id, optarg);
			application_flag = 1;
			log_info("> application_id : %s\n", application_id);
			break;
		case 'e':
			strcpy(experiment_id, optarg);
			experiment_flag = 1;
			log_info("> experiment_id : %s\n", experiment_id);
			break;
		case 'c':
			strcpy(component_id, optarg);
			component_flag = 1;
			log_info("> component_id : %s\n", component_id);
			break;
		case 'h':
			help = 1;
			break;
		case '?':
			err = 1;
			break;
	}
	/* print usage */
	if (err || help) {
		char usage[256] = {'\0'};
		sprintf(usage, "usage: %s [-a application_id] [-e experiment_id] [-c component_id] [-h help]\n", argv[0]);
		log_error("%s", usage);
		exit(FAILURE);
	}
	if(application_flag == 0) {
		strcpy(application_id, DEFAULT_APP);
	}
	if(experiment_flag == 0) {
		strcpy(experiment_id, DEFAULT_EXP);
	}
	if(component_flag == 0) {
		strcpy(component_id, DEFAULT_COMP);
	}
	
	/* set the configuration file */
	sprintf(confFile, "%s/%s", pwd, "../mf_config.ini");
	log_info("Configuration taken from: %s.\n", confFile);
	
	/* try to parse configuration file */
	if (mfp_parse(confFile) == FAILURE) {
		/* could not parse configuration file */
		fprintf(logFile, "[ERROR] Stopping service...could not parse configuration.\n");
		exit(FAILURE);
	}

	if (prepare() == FAILURE) {
		fprintf(logFile, "[ERROR] Stopping service...could not prepare URLs for sending metrics to server.\n");
		exit(FAILURE);
	}

	if(startThreads() == FAILURE) {
		fprintf(logFile, "[ERROR] Stopping service...could not start the threads.\n");
		exit(FAILURE);
	}

	/* clean up tasks */
	fprintf(logFile, "[INFO] Stopping service ...\n");
	fclose(logFile);
	
	unlink(name); // delete the file which stores the tmp pid
	free(pwd);
	free(name);

	free(application_id);
	free(experiment_id);
	free(component_id);
	free(metrics_publish_URL);
	free(platform_id);

	free(confFile);
	mfp_parse_clean();

	exit(SUCCESS);
}

/* assigns the current working directory to a variable */
void set_pwd(void)
{
	if (pwd_is_set) {
		return;
	}
	char buf_1[256] = {'\0'};
	char buf_2[256] = {'\0'};
	
	readlink("/proc/self/exe", buf_1, 200);
	memcpy(buf_2, buf_1, strlen(buf_1) * sizeof(char));

	/* extract path folder of executable from it's path */
	char *lastslash = strrchr(buf_2, '/');
	int ptr = lastslash - buf_2;

	memcpy(pwd, buf_2, ptr);
	pwd_is_set = 1;
}

/* write PID to a file (named as "tmp_pid") in `pwd` */
int writeTmpPID(void) 
{
	if (!pwd_is_set) {
		set_pwd();
	}

	strcpy(name, pwd);
	strcat(name, "/tmp_pid");

	int pid = getpid();
	FILE *tmpFile = fopen(name, "w");
	if (tmpFile == NULL) {
		log_error("Failed to create file %s to store the PID.\n", name);
		return FAILURE;
	} else {
		fprintf(tmpFile, "%d", pid);
		fclose(tmpFile);
	}

	return SUCCESS;
}

/* Initializing and creating the log file */
void createLogFile(void) 
{
	int errnum;

	if (!pwd_is_set) {
		set_pwd();
	}
	char logFileName[300] = { '\0' };
	char logFileFolder[300] = { '\0' };

	time_t curTime;
	time(&curTime);
	struct tm *time_info = localtime(&curTime);

	char timeForFile[50];

	strftime(timeForFile, 50, "%F-%T", time_info);
	sprintf(logFileFolder, "%s/log", pwd);
	sprintf(logFileName, "%s/log/log-%s", pwd, timeForFile);
	fprintf(stderr, "using logfile: %s\n", logFileName);

	struct stat st = { 0 };
	if (stat(logFileFolder, &st) == -1)
		mkdir(logFileFolder, 0700);
	logFile = fopen(logFileName, "w");
	if (logFile == NULL) {
		errnum = errno;
		fprintf(stderr, "Could not create log file: %s\n", logFileName);
		fprintf(stderr, "Error creating log: %s\n", strerror(errnum));
	} else {
		log_info("Starting service at %s...\n", timeForFile);
	}
}

/* prepare metrics_publish_URL and platform_id, based on mf_config.ini*/
int prepare(void)
{
	char server_name[128] = {'\0'};
	
	metrics_publish_URL = calloc(256, sizeof(char));
	platform_id = calloc(128, sizeof(char));
	/* get server */
	mfp_get_value("generic", "server", server_name);
	/* get metrics send url */
	sprintf(metrics_publish_URL, "%s/dreamcloud/mf/metrics", server_name);

	/* get platform_id */
	mfp_get_value("generic", "platform_id", platform_id);

	return SUCCESS;
}