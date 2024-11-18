#ifndef DB_HELPER_H
#define DB_HELPER_H

// include files
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <string.h>

// function prototypes
int dbase_init(const char *db_name);
int dbase_append(const char *db_name, const char *field);
int dbase_query(const char *db_name);
void dbase_append_double_array(const char *db_name, double *array, int size) ;
void append_samples_to_db(const char *db_name, double *samples, int num_samples);
void append_sample_string_to_db(const char *db_name, double *samples, int num_samples) ;
char* convert_samples_to_string(double *samples, int num_samples);
#endif // DB_HELPER_H