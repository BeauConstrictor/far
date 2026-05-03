#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

#include "parser.h"

#define USAGE_MSG "Usage: far [OPERATION] [IN] -o [OUT]\n"                               \
                  "Try 'far --help' for more information\n"

#define HELP_MSG  "Usage: far [OPERATION] [IN] -o [OUT]\n"                               \
                  "Read and write far archives.\n"                                       \
                  "\n"                                                                   \
                  "\t--help     show this message and exit\n"                            \
                  "\t--version  show version number and license, and exit\n"             \
                  "\n"                                                                   \
                  "OPERATIONS:\n"                                                        \
                  "\t-x         extract archive IN ('%%' for stdin), into new dir OUT\n" \
                  "\t-p         pack dir IN into archive OUT ('%%' for stdout)\n"        \
                  "\t-l         list all items in IN ('%%' for stdin) (do not pass -o)\n"

#define VERSN_MSG "far (File ARchiver) version 0.2.0\n"                                  \
                  "Licensed under the FOSS MIT license.\n"                               \
                  "\n"                                                                   \
                  "Written by Beau Constrictor; see\n"                                   \
                  "<https://github.com/beauconstrictor/far>\n"

typedef enum {
  op_extract,
  op_list,
  op_pack,
} t_op;

void err_msg(const char *msg) {
  fprintf(stderr, "far: %s\n", msg);
  exit(1);
}

void err_msg_far(far_errno errnum) {
  if (errnum >= sizeof(far_errmsgs)/sizeof(far_errmsgs[0])) {
    err_msg("unknown error");
  }
  err_msg(far_errmsgs[errnum]);
}

void perr_msg() {
  perror("far");
  exit(1);
}

char *entry_type_name(t_entry_type type) {
  switch (type) {
    case '-': return "file";
    case 'd': return "dir";
    case 'l': return "symlink";
    default:  return "unknown";
  }
}

bool dir_exists(const char *path) {
    struct stat info;

    if (stat(path, &info) != 0) {
        return false;
    }

    return (info.st_mode & S_IFDIR) != 0;
}

void permissions_string(int permissions, char *out) {
    const char chars[] = {'r', 'w', 'x'};
    
    for (int i = 0; i < 3; i++) {
        int digit = (permissions >> (6 - 3*i)) & 7;
        
        for (int j = 0; j < 3; j++) {
            if (digit & (1 << (2 - j)))
                *out++ = chars[j];
            else
                *out++ = '-';
        }
    }

    *out = '\0';
}

void list_archive(FILE *archive) {
  while (more_entries_exist(archive)) {

    far_errno errnum = 0;
    t_entry *entry = read_entry(archive, &errnum);
    if (errnum) err_msg_far(errnum);

    char timestamp[32];
    struct tm *tm_info = localtime(&entry->timestamp);
    strftime(timestamp, sizeof(timestamp), "%F %H:%M", tm_info);

    char permissions[10];
    permissions_string(entry->permissions, permissions);

    printf("%c%s %5"PRId64" %s %s\n",
      entry->type,
      permissions,
      entry->size,
      timestamp,
      entry->name
    );

    free_entry(entry);
  }
}

void join_path(char *buf, size_t n, const char *a, const char *b) {
    snprintf(buf, n, "%s/%s", a, b);
}

// path must start with /
bool detect_path_traversal(const char *_path) {
  char *path = strdup(_path);
  if (path == NULL) return true; // bad idea?
  size_t len = strlen(path);
  char *s;

  // first, check for *..
  if (len >= 2 && strcmp(path + len-2, "..") == 0) {
    free(path);
    return true;
  }

  // next, if on windows, replace \\ with /
  // NOTE: this program doesn't really support windows, but just in
  // case someone compiles it for windows, i wouldn.t want this
  // vulnerability sneaking through.
  #ifdef _WIN32
    s = path;
    while (*s) {
      if (*s == '\\') *s = '/';
      s++;
    }
  #endif

  // finally, check for *../*
  s = path;
  const char pattern[] = "../";
  int match = 0;
  while (*s) {
    if (*s == pattern[match]) {
      match++;
      if (match == sizeof(pattern)-1) {
        free(path);
        return true;
      }
    }
    s++;
  }
  
  free(path);
  return false;
}

void extract_archive(FILE *archive, const char *output) {
  if (dir_exists(output)) err_msg("directory exists");
  mkdir(output, 0755);

  while (more_entries_exist(archive)) {
    far_errno errnum = 0;
    t_entry *entry = read_entry(archive, &errnum);
    if (errnum) err_msg_far(errnum);

    char outpth[PATH_MAX];
    join_path(outpth, sizeof(outpth), output, entry->name);
    if (detect_path_traversal(outpth)) {
      fprintf(stderr, "potential dangerous path encountered; file "
                      "'%s' skipped.\n", outpth);
      free_entry(entry);
      continue;
    }

    switch (entry->type) {
      case file_entry:
        FILE *f = fopen(outpth, "wb");
        if (f == NULL) perr_msg();
        fwrite(entry->data, 1, entry->size, f);
        fclose(f);
        chmod(outpth, entry->permissions);
        break;

      case directory_entry:
        mkdir(outpth, 0755);
        chmod(outpth, entry->permissions);
        break;

      case symlink_entry:
        fprintf(stderr, "symlinks are currently unsupported.\n");
        break;

      default:
        fprintf(stderr, "unknown entry type encountered.\n");
    }

    // non-error text still goes to stderr in case they are writing
    // the archive to stdout.
    fprintf(stderr, "%s\n", entry->name);

    free_entry(entry);
  }
}

void pack_recurse(const char *base, const char *path, FILE *archive) {
  DIR *dir = opendir(path);
    if (!dir) {
        perror(path);
        return;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0)
            continue;

        char fullpath[PATH_MAX];
        join_path(fullpath, sizeof(fullpath), path, entry->d_name);

        struct stat st;
        if (stat(fullpath, &st) == -1) {
            perror(fullpath);
            continue;
        }
        uint16_t mode = st.st_mode & 0777;
        time_t mtime = st.st_mtime;


        char name[PATH_MAX];
        join_path(name, sizeof(name), base, entry->d_name);
        fprintf(stderr, "%s\n", name);

        if (S_ISDIR(st.st_mode)) {
          t_entry *arch_entry = create_entry(
            name, mtime, mode, 0, NULL, directory_entry);
          size_t size;
          void *buffer = serialise_entry(arch_entry, &size);
          fwrite(buffer, 1, size, archive);
          free(buffer);

          pack_recurse(name, fullpath, archive);
        } else {
          FILE *f = fopen(fullpath, "rb");
          if (!f) {
            perror(fullpath);
            exit(1);
          }

          fseek(f, 0, SEEK_END);
          size_t size = ftell(f);
          void *buffer = malloc(size);
          rewind(f);
          fread(buffer, 1, size, f);
          t_entry *arch_entry = create_entry(
              name, mtime, mode, size, buffer, file_entry);
          free(buffer);
          buffer = serialise_entry(arch_entry, &size);
          fwrite(buffer, 1, size, archive);
          free(buffer);

          fclose(f);
        }
    }

    closedir(dir);
}

void pack_archive(const char *input, const char *output) {
  FILE *archive;
  if (strcmp(output, "%") == 0) {
    archive = stdout;
  } else {
    archive = fopen(output, "wb");
    if (!archive) {
      perror("far");
      exit(1);
    }
  }

  fprintf(archive, "far");
  pack_recurse("", input, archive);
  fclose(archive);
}

void parse_args(int argc, char *argv[], char *input, char *output,
    t_op *operation) {
  if (argc == 2 && strcmp(argv[1], "--help") == 0) {
    printf(HELP_MSG);
    exit(0);
  }

  if (argc == 2 && strcmp(argv[1], "--version") == 0) {
    printf(VERSN_MSG);
    exit(0);
  }

  if (argc == 3 && strcmp(argv[1], "-l") == 0) {
    *operation = op_list;
    snprintf(input, PATH_MAX, "%s", argv[2]);
    return;
  }

  if (argc != 5) {
    printf(USAGE_MSG);
    exit(1);
  }

  if (strlen(argv[1]) != 2 || argv[1][0] != '-') {
    printf(USAGE_MSG);
    exit(1);
  }

  if (strcmp(argv[3], "-o") != 0) {
    printf(USAGE_MSG);
    exit(1);
  }

  snprintf(input,  PATH_MAX, "%s", argv[2]);
  snprintf(output, PATH_MAX, "%s", argv[4]);

  switch (argv[1][1]) {
    case 'x':
      *operation = op_extract;
      break;
    case 'p':
      *operation = op_pack;
      break;
    default:
      printf(USAGE_MSG);
      exit(1);
  }
}

int main(int argc, char *argv[]) {
  char input[PATH_MAX];
  char output[PATH_MAX];
  t_op operation;
  parse_args(argc, argv, input, output, &operation);

  if (operation == op_pack) {
    pack_archive(input, output);
    return 0;
  }

  FILE *archive;
  if (strcmp(input, "%") == 0) {
    archive = stdin;
  } else {
    archive = fopen(input, "rb");
    if (!archive) {
      perror("far");
      exit(1);
    }
  }
  if (!verify_archive(archive)) err_msg("not a far archive");

  if (operation == op_list)
    list_archive(archive);
  else if (operation == op_extract)
    extract_archive(archive, output);

  fclose(archive);

  return 0;
}
