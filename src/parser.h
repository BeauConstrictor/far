#ifndef PARSER_H
#define PARSER_H

#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>

typedef enum {
  far_success     = 0,
  far_malloc_fail = 1,
  far_bad_eof     = 2,
} far_errno;

extern const char *far_errmsgs[3];

typedef char t_entry_type;
#define file_entry '-'
#define directory_entry 'd'
#define symlink_entry 'l'

typedef struct {
  char *name;
  int64_t timestamp;
  t_entry_type type;
  uint16_t permissions;
  int64_t size;
  void *data;
} t_entry;

t_entry *read_entry(FILE *archive, far_errno *errnum);
void *serialise_entry(t_entry *entry, size_t *size);
t_entry *create_entry(const char *name, time_t timestamp,
                      uint16_t permissions, size_t size,
                      const void *data, t_entry_type type);
void free_entry(t_entry *entry);
// check an archive for the magic number at the beginning
bool verify_archive(FILE *archive);
bool more_entries_exist(FILE *archive);

#endif // PARSER_H
