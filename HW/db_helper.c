#include "db_helper.h"

// private constants
const char INIT_SQL_CMD[] = "CREATE TABLE IF NOT EXISTS data_table (\
    _id INTEGER PRIMARY KEY AUTOINCREMENT, \
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, \
    samples TEXT \
)";

const char APPEND_SQL_CMD[] = "INSERT INTO data_table (value) VALUES (?)";

const char QUERY_SQL_CMD[] = "SELECT * FROM data_table ORDER BY timestamp DESC LIMIT 1";

// initialize database
int dbase_init(const char *db_name) {  // Change to void
    sqlite3 *db;

    int result = sqlite3_open(db_name, &db);
    if (result != SQLITE_OK) {
        fprintf(stderr, "Error opening %s database: %s\n", db_name, sqlite3_errmsg(db));
        sqlite3_close(db);
        return 1;
    }

    result = sqlite3_exec(db, INIT_SQL_CMD, NULL, NULL, NULL);
    if (result != SQLITE_OK) {
        fprintf(stderr, "Error executing SQL command: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_close(db);
    return 0;
}

// append data to the table
int dbase_append(const char *db_name, const char *field) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int result = sqlite3_open(db_name, &db);

    if (result != SQLITE_OK) {
        fprintf(stderr, "Error opening %s database: %s\n", db_name, sqlite3_errmsg(db));
        return result;
    }

    result = sqlite3_prepare_v2(db, APPEND_SQL_CMD, -1, &stmt, NULL);
    if (result != SQLITE_OK) {
        fprintf(stderr, "Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return result;
    }

    result = sqlite3_bind_text(stmt, 1, field, -1, SQLITE_STATIC);
    if (result != SQLITE_OK) {
        fprintf(stderr, "Error binding data to SQL statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return result;
    }

    result = sqlite3_step(stmt);
    if (result != SQLITE_DONE) {
        fprintf(stderr, "Error executing SQL statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return result;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return SQLITE_OK;
}

// query last value from the table
int dbase_query(const char *db_name) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int last_value = -1;

    int result = sqlite3_open(db_name, &db);
    if (result != SQLITE_OK) {
        fprintf(stderr, "Error opening %s database: %s\n", db_name, sqlite3_errmsg(db));
        return result;
    }

    result = sqlite3_prepare_v2(db, QUERY_SQL_CMD, -1, &stmt, NULL);
    if (result != SQLITE_OK) {
        fprintf(stderr, "Error preparing SQL statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return result;
    }

    result = sqlite3_step(stmt);
    if (result == SQLITE_ROW) {
        const char *timestamp = (const char *)sqlite3_column_text(stmt, 1);
        printf("Data timestamp: %s\n", timestamp);
        last_value = sqlite3_column_int(stmt, 2);
    } else if (result == SQLITE_DONE) {
        printf("No data found.\n");
    } else {
        fprintf(stderr, "Error executing query: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return last_value;
}

void dbase_append_double_array(const char *db_name, double *array, int size) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    const char *APPEND_SQL_CMD = "INSERT INTO data_table (value) VALUES (?)";

    if (sqlite3_open(db_name, &db) != SQLITE_OK) {
        fprintf(stderr, "Error opening database %s\n", db_name);
        return;
    }

    if (sqlite3_prepare_v2(db, APPEND_SQL_CMD, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    for (int i = 0; i < size; i++) {
        sqlite3_bind_double(stmt, 1, array[i]);  // Bind each array element

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            fprintf(stderr, "Error executing statement: %s\n", sqlite3_errmsg(db));
        }

        sqlite3_reset(stmt);  // Reset the statement for the next iteration
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}


void append_samples_to_db(const char *db_name, double *samples, int num_samples) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO data_table (samples) VALUES (?);";

    if (sqlite3_open(db_name, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    // Bind the blob (raw data) to the SQL statement
    sqlite3_bind_blob(stmt, 1, samples, num_samples * sizeof(double), SQLITE_TRANSIENT);

    // Execute the SQL statement
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

char* convert_samples_to_string(double *samples, int num_samples) {
    int buffer_size = num_samples * 12 + 1; // Estimate 12 characters per double
    char *string = malloc(buffer_size);
    string[0] = '\0';

    for (int i = 0; i < num_samples; i++) {
        char sample_str[12];
        snprintf(sample_str, sizeof(sample_str), "%.2f", samples[i]);
        strcat(string, sample_str);

        if (i < num_samples - 1) {
            strcat(string, ",");
        }
    }
    return string;
}


void append_sample_string_to_db(const char *db_name, double *samples, int num_samples) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO data_table (samples) VALUES (?);";

    if (sqlite3_open(db_name, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return;
    }

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return;
    }

    char *sample_string = convert_samples_to_string(samples, num_samples);

    // Bind the text string to the SQL statement
    sqlite3_bind_text(stmt, 1, sample_string, -1, SQLITE_TRANSIENT);

    // Execute the SQL statement
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Failed to execute statement: %s\n", sqlite3_errmsg(db));
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
    free(sample_string);
}