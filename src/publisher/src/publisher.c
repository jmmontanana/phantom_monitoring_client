/*
 * Copyright (C) 2014-2015 University of Stuttgart
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

#include <stdlib.h>
#include <stdio.h>
#include <curl/curl.h>

#include "mf_debug.h"
#include "publisher.h"

#define SUCCESS 1
#define FAILED  0
 /*******************************************************************************
 * Variables Declarations
 ******************************************************************************/
struct curl_slist *headers = NULL;

/*******************************************************************************
 * Forward Declarations
 ******************************************************************************/
int check_URL(char *URL);
int check_message(char *message);
void init_curl(void);
CURL *prepare_publish(char *URL, char *message);

#ifndef DEBUG
static size_t write_non_data(void *buffer, size_t size, size_t nmemb, void *userp);
#endif

/* json-formatted data publish using libcurl
   return 1 on success; otherwise return 0 */
int publish_json(char *URL, char *message)
{
    if (!check_URL(URL) || !check_message(message)) {
        return FAILED;
    }
    CURL *curl = prepare_publish(URL, message);
    if (curl == NULL) {
        return FAILED;
    }

    #ifndef DEBUG
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_non_data);
    #endif

    CURLcode response = curl_easy_perform(curl);

    if (response != CURLE_OK) {
        const char *error_msg = curl_easy_strerror(response);
        log_error("publish(char *, Message) %s", error_msg);
        return FAILED;
    }

    curl_easy_cleanup(curl);
    return SUCCESS;
}

/* Check if the url is set 
   return 1 on success; otherwise return 0 */
int check_URL(char *URL)
{
    if (URL == NULL || *URL == '\0') {
        const char *error_msg = "URL not set.";
        log_error("publish(char *, Message) %s", error_msg);
        return FAILED;
    }
    return SUCCESS;
}

/* Check if the message is set 
   return 1 on success; otherwise return 0 */
int check_message(char *message)
{
    if (message == NULL || *message == '\0') {
        const char *error_msg = "message not set.";
        log_error("publish(char *, Message) %s", error_msg);
        return FAILED;
    }
    return SUCCESS;
}

/* Initialize libcurl; set headers format */
void init_curl(void)
{
    if (headers != NULL ) {
        return;
    }
    curl_global_init(CURL_GLOBAL_ALL);
    headers = curl_slist_append(headers, "Accept: application/json");
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "charsets: utf-8");
}

/* Prepare for using libcurl to write */
CURL *prepare_publish(char *URL, char *message)
{
    init_curl();
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, message);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long ) strlen(message));
    
    #ifdef DEBUG
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    #endif

    return curl;
}

/* Callback function for writing with libcurl */
#ifndef DEBUG
static size_t write_non_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    return size * nmemb;
}
#endif