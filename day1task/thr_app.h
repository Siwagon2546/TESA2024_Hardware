#ifndef THR_APP_H
#define THR_APP_H

// include files
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "MQTTClient.h"

// shared variables
extern pthread_cond_t  data_cond;
extern pthread_mutex_t data_cond_mutex;
extern char *shared_data;
extern pthread_cond_t  avg_data_cond;
extern pthread_mutex_t avg_data_cond_mutex;
extern long avg_shared_data;

// function prototypes
void *front_thr_fcn( void *ptr );
void *mid_thr_fcn( void *ptr );
void *end_thr_fcn( void *ptr );

#endif