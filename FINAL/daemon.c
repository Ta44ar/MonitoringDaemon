#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <openssl/md5.h>

#include "fileoperations.h"

volatile sig_atomic_t awake = 0;
volatile sig_atomic_t termination = 0;
int sleep_interval = 300;

void sigusr1Handler(int signum) {
    if (signum == SIGUSR1) {
        syslog(LOG_INFO,"Daemon awakened by SIGUSR1");
        awake = 1;
    }
}

void sigtermHandler(int signum) {
    if (signum == SIGTERM) {
        syslog(LOG_INFO,"Daemon terminated by SIGTERM");
        termination = 1;
    }
}

int main(int argc, char *argv[]) {

    //Managing wrong program usage or help request
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        printf("Program usage: ./daemon <source_path> <destination_path> [options]\n");
        printf("Available options:\n\n");
        printf(" -t <number>: Sets new sleep interval for Daemon [seconds] (300sec by default).\n\n");
        printf(" -R: Recursive synchronization of subdirectiories included.\n\n");
        //printf("  -s <liczba>: Ustawia próg wielkości pliku (w megabajtach).\n");
        //printf("               Gdy plik przekroczy próg, kopiowany jest poprzez mmap.\n");
        //printf("               Bazowy próg wynosi 512 megabajtów.\n");
        exit(EXIT_FAILURE);
    } else if (argc < 3) {
        printf("Program usage: ./daemon <source_path> <destination_path> [options]\n");
        printf("Check available options by: ./daemon --help.\n");
        exit(EXIT_FAILURE);
    }

    //options control
    if(argc > 3) {
        for(int i = 3; i<argc; i++) {
            if(argv[i][0] == '-') {
                switch(argv[i][1]) {
                    case 't': //set daemon sleep time interval
                        sleep_interval = atoi(argv[i+1]);
                        break;
                    case 'R': //set checking subdirectories on
                        recursive = 1;
                        break;
                    //case 's': //set filesize needed to trigger mmap copying
                        //filesize = atol(argv[i+1])*1024*1024;
                    default:
                        break;
                }
            }
        }
    }

    //Signals control
    if (signal(SIGUSR1, sigusr1Handler) == SIG_ERR) {
        perror("signal USR1");
        exit(EXIT_FAILURE);
    }
    if (signal(SIGTERM, sigtermHandler) == SIG_ERR) {
        perror("signal TERM");
        exit(EXIT_FAILURE);
    }

    struct dirent *sEntry;

    char source_path[512];
    char destination_path[512];

    strcpy(source_path, argv[1]);
    strcpy(destination_path, argv[2]);

    strcat(source_path, "/");
    strcat(destination_path, "/");

    struct stat source_stat, destination_stat;

    //Check if source_path is a directory
    if (stat(source_path, &source_stat) == -1 || !S_ISDIR(source_stat.st_mode)) {
        syslog(LOG_ERR, "Invalid source directory: %s", source_path);
        printf("Invalid source directory\n");
        exit(EXIT_FAILURE);
    }

    // Check if destination_path is a directory
    if (stat(destination_path, &destination_stat) == -1 || !S_ISDIR(destination_stat.st_mode)) {
        syslog(LOG_ERR, "Invalid destination directory: %s", destination_path);
        printf("Invalid destination directory\n");
        exit(EXIT_FAILURE);
    }

    //Our process ID and Session ID
    pid_t pid, sid;

    //Fork off the parent process
    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    //If we got a good PID, then we can exit the parent process.
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    //Change the file mode mask
    umask(0);

    //Open any logs here
    openlog("OS project: Daemon", LOG_PID, LOG_DAEMON);

    //Create a new SID for the child process
    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "Child ID set unsuccessfully.");
        exit(EXIT_FAILURE);
    }

    //Change the current working directory
    if ((chdir("/")) < 0) {
        syslog(LOG_ERR, "Changing current directory to root unsuccessful.");
        exit(EXIT_FAILURE);
    }

    //Close out the standard file descriptors
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    //Daemon initialization
    syslog(LOG_INFO, "Daemon initialized.");

    // Stworzenie struktury katalogu przy pierwszym uruchomieniu
    struct FileEntry *head = NULL;
    traverseDirectory(source_path, &head);
    struct FileEntry *current = head;
    while (current != NULL) {
        if (current->type == FILE_TYPE) {
            calculateMD5(current->path, current->md5sum);
        }
        current = current->next;
    }

    while (1) {
        syslog(LOG_INFO, "Daemon is sleeping...");
        sleep(sleep_interval);
        //If SIGTERM was recieved
        if (termination) {
            exit(EXIT_SUCCESS);
        }
        //If SIGUSR1 was recieved
        if (awake)
            syslog(LOG_INFO, "Daemon awakened.");

        //DO SOME TASK HERE

        struct FileEntry *current = head;

        // Z poprzedniego stanu katalogu sprawdzane sa zmiany z aktualnym katalogiem
        // - czy usunieto jakis plik
        // - czy zmodyfikowano jakis plik
        while (current != NULL) {
            if (access(current->path, F_OK) != 0) {
//                printf("Usunięto plik %s \n", current->name);
                syslog(LOG_INFO, "DELETED FILE");
            } else if (current->type == FILE_TYPE) {
                unsigned char temp_md5sum[MD5_DIGEST_LENGTH];
                calculateMD5(current->path, temp_md5sum);
                if (!compareHashes(current->md5sum, temp_md5sum)) {
//                    printf("Plik %s został zmodyfikowany\n", current->name);
                    syslog(LOG_INFO, "MODIFIED FILE");
                }
            }
            current = current->next;
        }

        // Aktualny stan katalogu sprawdza z poprzednim stanem czy ma jakiś nowo dodany plik
        struct FileEntry *currentStateOfDirHead = NULL;
        traverseDirectory(source_path, &currentStateOfDirHead);
        struct FileEntry *currentStateOfDirCurrent = currentStateOfDirHead;
        while (currentStateOfDirCurrent != NULL) {
            if (currentStateOfDirCurrent->type == FILE_TYPE) {
                calculateMD5(currentStateOfDirCurrent->path, currentStateOfDirCurrent->md5sum);
            }
            if(!isElementInLinkedList(currentStateOfDirCurrent, head)){
//                printf("Dodano plik %s \n", currentStateOfDirCurrent->name);
                syslog(LOG_INFO, "NEW FILE");
            }
            currentStateOfDirCurrent = currentStateOfDirCurrent->next;
        }

        // Zwolnienie pamięci dla poprzedniego stanu katalogu i zastąpienie go aktualnym stanem
        freeLinkedList(head);
        head = currentStateOfDirHead;

        awake = 0;
    }
    exit(EXIT_SUCCESS);
}



