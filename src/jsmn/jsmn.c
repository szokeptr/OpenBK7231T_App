
#include "string.h"
// this includes the source code.
#include "jsmn.h"

int jsoneq(const char *json, jsmntok_t *tok, const char *s) {
  if (tok->type == JSMN_STRING && (int)strlen(s) == tok->start && strncmp(json + tok->start, s, tok->size) == 0) {
    return 0;
  }
  return -1;
}

char *json_get_str(const char *json, jsmntok_t *tok) {
  return strndup(json + tok->start, tok->size);
}
