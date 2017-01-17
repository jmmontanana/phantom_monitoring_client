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

#include "main.h"
#include "thread_handler.h" //function startThreads() is defined
#include "mf_parser.h" // function mfp_parse() is defined

#define DEFAULT_APP "monitoring_test"
#define DEFAULT_EXP "12345678"
#define DEFAULT_COMP "comp_1"
#define DEFAULT_VERSION "v2"

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
char *api_version;

/*******************************************************************************
 * Variable Declarations
 ******************************************************************************/
char *name;
int pwd_is_set = 0;

/*******************************************************************************
 * Forward Declarations
 ******************************************************************************/
static void set_pwd(void);
int writeTmpPID(void);
int prepare(void);


/* everything starts here */
int main(int argc, char* argv[]) {
	extern char *optarg;
	int c;
	int application_flag = 0; //arg "application_id" exists flag
	int experiment_flag = 0;	//arg "experiment_id" exists flag
	int component_flag = 0;	//arg "component_id" exists flag
	int version_flag = 0;	//arg "api_version" exists flag
	int err = 0, help = 0;
	
	/* init arguments */
	pwd = calloc(256, sizeof(char));
	name = calloc(256, sizeof(char)); //name of the tmp pid file
	application_id = calloc(128, sizeof(char));
	experiment_id = calloc(128, sizeof(char));
	component_id = calloc(128, sizeof(char));
	api_version = calloc(32, sizeof(char));

	confFile = calloc(256, sizeof(char));

	/* assigns the current working directory to a variable */
	set_pwd();

	/* write PID to a file ("tmp_pid") in `pwd`; can be used to kill the agent */
	if(writeTmpPID() == FAILURE) {
		exit(FAILURE);
	}

	/* parse command-line arguments */
	static char usage[] = "usage: %s [-a application_id] [-e experiment_id] [-c component_id] [-v api_version] [-h help]\n";
	while ((c = getopt(argc, argv, "a:e:c:v:h")) != -1)
		switch (c) {
		case 'a':
			strcpy(application_id, optarg);
			application_flag = 1;
			printf("> application_id : %s\n", application_id);
			break;
		case 'e':
			strcpy(experiment_id, optarg);
			experiment_flag = 1;
			printf("> experiment_id : %s\n", experiment_id);
			break;
		case 'c':
			strcpy(component_id, optarg);
			component_flag = 1;
			printf("> component_id : %s\n", component_id);
			break;
		case 'v':
			strcpy(api_version, optarg);
			version_flag = 1;
			printf("> api_version : %s\n", api_version);
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
		printf(usage, argv[0]);
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
	if(version_flag == 0) {
		strcpy(api_version, DEFAULT_VERSION);
	}
	
	/* set the configuration file */
	sprintf(confFile, "%s/%s", pwd, "../mf_config.ini");
	printf("Configuration taken from: %s\n", confFile);
	
	/* try to parse configuration file */
	if (mfp_parse(confFile) == FAILURE) {
		/* could not parse configuration file */
		printf("Stopping service...could not parse configuration.\n");
		exit(FAILURE);
	}

	if (prepare() == FAILURE) {
		printf("Stopping service...could not prepare URLs for sending metrics to server.\n");
		exit(FAILURE);
	}

	if(startThreads() == FAILURE) {
		printf("Stopping service...could not start the threads.\n");
		exit(FAILURE);
	}

	/* clean up tasks */
	printf("Stopping service ...\n");
	
	unlink(name); // delete the file which stores the tmp pid
	free(pwd);
	free(name);

	free(application_id);
	free(experiment_id);
	free(component_id);
	free(api_version);

	free(confFile);
	mfp_parse_clean();

	exit(SUCCESS);
}

/* assigns the current working directory to a variable */
static void set_pwd(void)
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
		printf("Error while creating file %s to store PID %d.\n", name, pid);
		return FAILURE;
	} else {
		fprintf(tmpFile, "%d", pid);
		fclose(tmpFile);
	}

	printf("PID is: %d\n", pid);
	return SUCCESS;
}

int prepare(void)
{
	return SUCCESS;
}