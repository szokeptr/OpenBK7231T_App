// prevent includsion of the source code
#define JSMN_HEADER
#include "string.h"
#include "jsmn.h"

int jsoneq(const char *json, jsmntok_t *tok, const char *s);

int json_get_str(const char *json, jsmntok_t *tok);
