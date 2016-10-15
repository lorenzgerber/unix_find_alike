#define _POSIX_C_SOURCE 200809L
#include "mfind.h"
#include <pthread.h>
#include <limits.h>
#include <sys/stat.h>

pthread_mutex_t mtx_pathList;
pthread_mutex_t mtx_resultList;
int waitCount = 0;
sem_t pathCount;
int nrThreads = 0;
list *pathList;
list *resultList;
char type;
char *toMatch;


void pathRecordFree(void *recordToFree){
    pathRecord *record = (pathRecord*)recordToFree;
    free(record->path);
    free(record);
}
void resultRecordFree(void *recordToFree){
    pathRecord *record = (pathRecord*)recordToFree;
    free(record->path);
    free(record);
}


int main(int argc, char *argv[]) {

    /*
     * Initialize variables
     */
    int opt;

    int typeFound;
    int nrArgPath, pathFound;

    pathList = listEmpty();
    listSetMemHandler(pathList, pathRecordFree);
    resultList = listEmpty();
    listSetMemHandler(resultList, resultRecordFree);

    typeFound = 0;
    pathFound = 0;
    nrArgPath = 0;
    pthread_mutex_init(&mtx_pathList, NULL);
    pthread_mutex_init(&mtx_resultList, NULL);

    /*
     * Parse commandline options
     *
     * mfind [-t type] [-p nrThreads] start1 [start ...] name
     *
     */
    while ((opt = getopt(argc, argv, "t:p:")) != -1) {
        switch (opt) {
            case 't':
                type = *optarg;
                typeFound = 1;
                break;
            case 'p':

                nrThreads = atoi(optarg);
                pathFound = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-t nsecs] [-n] name\n",
                        argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if(typeFound){
        printf("type set to %c\n", type);
    }

    if(pathFound){
        printf("number of threads %d\n", nrThreads);
    }

    if (optind >= argc) {
        fprintf(stderr, "Expected argument after options\n");
        exit(EXIT_FAILURE);
    }

    /*
     * are the provided path'es valid
     */
    for (int iii = argc - 2 ; iii >= optind; iii--){
        struct stat statbuf;
        if (stat(argv[iii], &statbuf) < 0){
            fprintf(stderr, "lstat: %s\n", argv[iii]);
            perror("");
            exit(EXIT_FAILURE);
        }
        if (S_ISDIR(statbuf.st_mode) == 0){
            fprintf(stderr,"%s is not or does not link to a directory\n", argv[iii]);
            exit(EXIT_FAILURE);
        }

        char* insertionPath = strdup(argv[iii]);
        pathRecord *insertionRecord = malloc(sizeof(pathRecord) * 1);
        insertionRecord->path = insertionPath;
        insertionRecord->searched = false;
        listInsert(listLast(pathList), (data)insertionRecord);
        nrArgPath++;

    }
    /*
     * String to match
     */
    toMatch = strdup(argv[argc-1]);

    /*
     * Initialize nrPath Semaphore
     */
    //sem_t* pathCount;
    if(sem_init(&pathCount, 0, nrArgPath)<0){
        perror("sem_init");
        exit(EXIT_FAILURE);
    }


    /*
     * Start all threads
     */
    pthread_t threads[nrThreads];
    threads[0] = pthread_self();

    for(int iii = 1; iii < nrThreads;iii++){
        if(pthread_create(&threads[iii], NULL, &threadMain, NULL)){
            perror("pthread");
            exit(EXIT_FAILURE);
        }
    }

    /*
     * start main function for master thread
     */
    threadMain(NULL);


    /*
     * wait for all threads to finish
     */
    for(int iii = 1; iii < nrThreads;iii++){
        if(pthread_join(threads[iii], NULL)){
            perror("pthread_join");
        }
    }

    /*
     * print the results
     */
    printf("this is the shit\n");
    printf("%ld", pathmax);
    exit(EXIT_SUCCESS);
}



/*
 * Thread main function
 *
 * 1. Put all start folders in 'folder list' to be traversed
 *
 * 2. working function
 * - take a map from list of folders
 * - read entries, write relevant into result list
 * - if entry is a map, put into folder list
 * - increase counter of folders searched
 * - start again
 *
 */
void *threadMain(void *dummy){
    printf("in the thread\n");

    // increase waitCount initially;
    waitCount++;
    int gotWork;


    // need to put this all in a do while waitCount < nrThreads
    do{
        // tryWait on pathCount Semaphore
        gotWork = sem_trywait(&pathCount);
        if(gotWork==0){
            printf("we got some work\n");
            waitCount--;
            if(readDir()<0){
                fprintf(stderr,"readDir: something wrong\n");
            }
        } else if (errno!=EAGAIN){
            perror("sem_trywait");
        } else if (errno==EAGAIN){
            //printf("currently no work\n");
        }

    } while (waitCount < nrThreads);

    printf("seems like everybody finished\n");


    return NULL;
}

/*
 * adding a path to a the pathList
 */
int readDir(void){
    // lock the pathList
    pthread_mutex_lock(&mtx_pathList);
    printf("pathlist locked\n");
    listPosition currentPosition = listFirst(pathList);
    bool foundOrEnd = false;

    // find path to work on
    while(foundOrEnd == false){
        if(((pathRecord*)listInspect(currentPosition))->searched == false){
            printf("we have one to work on\n");
            // mark path as done
            pathRecord* workRecord = (pathRecord*)listInspect(currentPosition);
            workRecord->searched = true;

            foundOrEnd = true;
        }
        if(listIsEnd(pathList,currentPosition)==true || foundOrEnd == true){
            foundOrEnd = true;
        } else {
            currentPosition = listNext(currentPosition);
        }
    }

    // unlock pathlist
    pthread_mutex_unlock(&mtx_pathList);
    printf("pathlist unlocked\n");

    // loop through directory entries
    struct stat statbuf;
    struct dirent *dirp;
    DIR *dp;
    char * currentPath = ((pathRecord*)listInspect(currentPosition))->path;

    if((dp = opendir(currentPath))==NULL){
        printf("can't read directory");
    }

    while ((dirp= readdir(dp)) != NULL){
        if (strcmp(dirp->d_name, ".") == 0  ||
            strcmp(dirp->d_name, "..") == 0)
            continue;
        printf("%s %d\n", dirp->d_name, (int) strlen(currentPath));

        /*
         * prepare path + name
         */
        size_t pathLength = strlen(currentPath);
        size_t fileLength = strlen(dirp->d_name);
        int multiplier = 1;
        while((pathLength+fileLength+2) > (pathmax * multiplier)){
            multiplier ++;
        }
        char* fullpath = calloc(pathmax * multiplier, sizeof(char));
        strcpy(fullpath, currentPath);
        fullpath[pathLength++] = '/';
        strcpy(&fullpath[pathLength], dirp->d_name);
        //printf("%s\n", fullpath);

        //checkmatch
        if(strcmp(dirp->d_name, toMatch)==0){
            printf("WE GOT A MATCH\n");
        };


        if(lstat(fullpath, &statbuf)<0){
            perror("lstat");
        };

        //if directory found
        if (S_ISDIR(statbuf.st_mode) == 1){
            printf("%s is a directory\n", fullpath);
            // check match
            //if match found
            //mutex lock resultList
            pthread_mutex_lock(&mtx_resultList);
            printf("resultlist locked\n");
            //add to resultList
            //mutex unlock resultList
            pthread_mutex_unlock(&mtx_resultList);
            printf("resultlist unlocked\n");


            //mutex lock pathList
            pthread_mutex_lock(&mtx_pathList);
            printf("pathlist locked\n");

            //add directory to path list

            //semaphore post

            //sem_post(&pathCount);

            //mutex unlock pathlist
            pthread_mutex_unlock(&mtx_pathList);
            printf("pathlist unlocked\n");

        } else if(S_ISREG(statbuf.st_mode) ==1){
            printf("%s is a file\n", fullpath);
            // check match
            //if match found
            //mutex lock resultList
            pthread_mutex_lock(&mtx_resultList);
            printf("resultlist locked\n");
            //add to resultList
            //mutex unlock resultList
            pthread_mutex_unlock(&mtx_resultList);
            printf("resultlist unlocked\n");

        } else if(S_ISLNK(statbuf.st_mode) == 1){
            printf("%s is a link\n", fullpath);
            // check match
            //if match found
            //mutex lock resultList
            pthread_mutex_lock(&mtx_resultList);
            printf("resultlist locked\n");
            //add to resultList
            //mutex unlock resultList
            pthread_mutex_unlock(&mtx_resultList);
            printf("resultlist unlocked\n");

        }


    }
    if (closedir(dp) < 0){
        perror("closedir");
    }

    waitCount++;
    return 0;
}
