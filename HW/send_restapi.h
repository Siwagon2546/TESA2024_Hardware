#ifndef SEND_RESTAPI_H
#define SEND_RESTAPI_H

#include <stddef.h>

// Function declarations
char *base64_encode(const unsigned char *input, int length);
int sendrest_api(char *json_data);

#endif // SEND_RESTAPI_H
