#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <getopt.h>
#include <pthread.h>
#include <stdbool.h>
#include <limits.h>
#include <sys/stat.h>
#include <semaphore.h>

#include "list.h"

#ifdef PATH_MAX
static long pathmax = PATH_MAX;
#else
static long pathmax = 0;
#endif

static int waitCount = 0;
static sem_t pathCount;
static int nrThreads = 0;

typedef struct pathRecord {
    char* path;
    bool searched;
} pathRecord;

typedef struct resultRecord{
    char* path;
} resultRecord;

void *threadMain(void *dummy);

void pathRecordFree(void *recordToFree);

void resultRecordFree(void *recordToFree);

int readDir(void);