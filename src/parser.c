#include <time.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>

#include "parser.h"

#define MAGIC "far"

const char *far_errmsgs[] = {
  "success",
  "unable to allocate memory",
  "unexpected eof",
};

static void give_err(far_errno *errnum, far_errno errtype) {
  if (errnum == NULL) return;
  *errnum = errtype;
}

static size_t read_str(FILE *f, size_t cap, char *buf) {
  size_t len = 0;
  int c = 0;

  while (len < cap-1) {
    c = fgetc(f);
    if (c == 0 || c == EOF) break;
    buf[len] = (char)c;
    len++;
  }

  buf[len] = '\0';
  return len;
}

static int64_t read_int(FILE *f) {
  char s[128];
  read_str(f, sizeof(s), s);
  return strtol(s, NULL, 10);
}

t_entry *read_entry(FILE *archive, far_errno *errnum) {
  size_t n;

  t_entry *entry = malloc(sizeof(t_entry));
  if (!entry) {
    give_err(errnum, far_malloc_fail);
    free_entry(entry);
    return NULL;
  }

  entry->name = malloc(PATH_MAX);
  if (!entry->name) {
    give_err(errnum, far_malloc_fail);
    free_entry(entry);
    return NULL;
  }
  read_str(archive, PATH_MAX, entry->name);

  entry->timestamp = read_int(archive);

  int type = fgetc(archive);
  if (type == EOF) {
    give_err(errnum, far_bad_eof);
    free_entry(entry);
    return NULL;
  }
  entry->type = (char)type;

  char permission_str[4];
  n = fread(permission_str, 1, 3, archive);
  if (n != 3) {
    give_err(errnum, far_bad_eof);
    free_entry(entry);
    return NULL;
  }
  permission_str[3] = '\0';
  entry->permissions = strtol(permission_str, NULL, 8);

  entry->size = read_int(archive);

  void *data = malloc(entry->size);
  if (!data) {
    give_err(errnum, far_malloc_fail);
    free_entry(entry);
    return NULL;
  }
  entry->data = data;
  n = fread(data, 1, entry->size, archive);
  if (n != (size_t)entry->size) {
    give_err(errnum, far_bad_eof);
    free_entry(entry);
    return NULL;
  }

  return entry;
}

void *serialise_entry(t_entry *entry, size_t *bufsize) {
  char timestamp[128];
  snprintf(timestamp, sizeof(timestamp), "%"PRId64, entry->timestamp);

  char permissions[7];
  snprintf(permissions, sizeof(permissions), "%o", entry->permissions);
  permissions[3] = '\0';

  char size[128];
  snprintf(size, sizeof(size), "%"PRIu64, entry->size);

  size_t namelen = strlen(entry->name);
  size_t timestamplen = strlen(timestamp);
  size_t sizelen = strlen(size);

  *bufsize = 0;
  *bufsize += namelen + 1;
  *bufsize += timestamplen + 1;
  *bufsize += 1 + 3;
  *bufsize += sizelen + 1;
  *bufsize += entry->size;
  void *buf = malloc(*bufsize);
  char *head = buf;

  memcpy(head, entry->name, namelen+1);
  head += namelen+1;

  memcpy(head, timestamp, timestamplen+1);
  head += timestamplen+1;

  *head = entry->type;
  head++;

  memcpy(head, permissions, 3);
  head += 3;

  memcpy(head, size, sizelen+1);
  head += sizelen+1;

  memcpy(head, entry->data, entry->size);
  head += entry->size;

  return buf;
}

t_entry *create_entry(const char *name, time_t timestamp,
                      uint16_t permissions, size_t size,
                      const void *data, t_entry_type type) {
  t_entry *entry = malloc(sizeof(t_entry));

  entry->name = strdup(name);
  entry->timestamp = (int64_t)timestamp;
  entry->permissions = permissions;
  entry->type = type;
  entry->size = size;
  entry->data = malloc(size);
  memcpy(entry->data, data, size);

  return entry;
}

void free_entry(t_entry *entry) {
  free(entry->name);
  free(entry->data);
  free(entry);
}

bool verify_archive(FILE *archive) {
  char magic[4];
  size_t n = fread(magic, 1, sizeof(magic)-1, archive);
  magic[3] = '\0';
  return n == sizeof(magic)-1 && strcmp(magic, MAGIC) == 0;
}

bool more_entries_exist(FILE *archive) {
    int c = fgetc(archive);
    if (c == EOF) return false;
    if (ungetc((char)c, archive) == EOF) return false;
    return true;
}


