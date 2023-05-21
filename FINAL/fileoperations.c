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
#include <libgen.h>

#include "fileoperations.h"

int recursive = 0;

struct FileEntry *createFileEntry(const char *name, const char *path, enum FileType type) {
    struct FileEntry *entry = (struct FileEntry *) malloc(sizeof(struct FileEntry));
    strncpy(entry->name, name, sizeof(entry->name));
    strncpy(entry->path, path, sizeof(entry->path));
    entry->type = type;
    entry->next = NULL;
    return entry;
}

void insertFileEntry(struct FileEntry **head, struct FileEntry *entry) {
    if (*head == NULL) {
        *head = entry;
    } else {
        struct FileEntry *current = *head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = entry;
    }
}

void calculateMD5(const char *filePath, unsigned char *md5sum) {
    FILE *file = fopen(filePath, "rb");
    if (file == NULL) {
        printf("Unable to open file: %s\n", filePath);
        return;
    }

    MD5_CTX context;
    MD5_Init(&context);

    unsigned char buffer[1024];
    int bytesRead;
    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) != 0) {
        MD5_Update(&context, buffer, bytesRead);
    }

    MD5_Final(md5sum, &context);

    fclose(file);
}

int compareHashes(const unsigned char *hash1, const unsigned char *hash2) {
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        if (hash1[i] != hash2[i]) {
            return 0;
        }
    }
    return 1;
}

void traverseDirectory(const char *path, struct FileEntry **head) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        printf("Unable to open directory: %s\n", path);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char filePath[1024];
        if (path[strlen(path) - 1] == 47){
            snprintf(filePath, sizeof(filePath), "%s%s", path, entry->d_name);
        }
        else{
            snprintf(filePath, sizeof(filePath), "%s/%s", path, entry->d_name);
        }

        if (entry->d_type == DT_DIR) {
            struct FileEntry *newEntry = createFileEntry(entry->d_name, filePath, DIRECTORY_TYPE);
            insertFileEntry(head, newEntry);
            if(recursive) traverseDirectory(filePath, head);
        } else if (entry->d_type == DT_REG) {
            struct FileEntry *newEntry = createFileEntry(entry->d_name, filePath, FILE_TYPE);
            insertFileEntry(head, newEntry);
        }
    }

    closedir(dir);
}

void freeLinkedList(struct FileEntry *head) {
    while (head != NULL) {
        struct FileEntry *current = head;
        head = head->next;
        free(current);
    }
}

int isElementInLinkedList(struct FileEntry* element, struct FileEntry* head) {
    while (head != NULL) {
        if (strcmp(element->name, head->name) == 0 &&
            strcmp(element->path, head->path) == 0 &&
            //            strcmp(element->md5sum, head->md5sum) == 0 &&
            element->type == head->type) {
            return 1;
        }
        head = head->next;
    }
    return 0;
}

char* absoluteToRelative(const char* absolutePath, const char* currentFolder) {
    // Sprawdzanie poprawności argumentów
    if (absolutePath == NULL || currentFolder == NULL) {
        return NULL;
    }

    // Obliczanie długości folderów
    size_t absoluteLen = strlen(absolutePath);
    size_t currentLen = strlen(currentFolder);

    // Obliczanie różnicy między folderami
    size_t i = 0;
    while (i < absoluteLen && i < currentLen && absolutePath[i] == currentFolder[i]) {
        i++;
    }

    // Liczenie liczby folderów w folderze bieżącym
    size_t numDirs = 0;
    for (; i < currentLen; i++) {
        if (currentFolder[i] == '/') {
            numDirs++;
        }
    }

    // Tworzenie ścieżki względnej
    size_t relativeLen = 3 * numDirs + absoluteLen - i + 1;  // 3 * numDirs dla "../" oraz 1 dla '\0'
    char* relativePath = (char*)malloc(relativeLen * sizeof(char));
    if (relativePath == NULL) {
        return NULL;
    }

    strcpy(relativePath, "");
    for (size_t j = 0; j < numDirs; j++) {
        strcat(relativePath, "../");
    }
    strcat(relativePath, absolutePath + i);

    return relativePath;
}

void getDestinationFilePath(char* temp_path, char* destination_path, char* current_path, char* source_path) {

    temp_path[0] = '\0';
    strcat(temp_path, destination_path);
    strcat(temp_path, absoluteToRelative(current_path, source_path));
}

void createDirectories(char* path, int delete_file_from_path) {
    char* pathCopy = strdup(path);  // Kopiujemy ścieżkę do osobnej zmiennej, aby nie modyfikować oryginalnej
    if(delete_file_from_path) 
        pathCopy = dirname(pathCopy);
    char* token = strtok(pathCopy, "/");  // Rozdzielamy ścieżkę na poszczególne katalogi
    
    char currentPath[256] = "";  // Inicjalizujemy pusty aktualny katalog
    
    while (token != NULL) {
        // Dodajemy kolejny katalog do aktualnej ścieżki
        strcat(currentPath, token);
        strcat(currentPath, "/");
        
        // Tworzymy katalog, jeśli nie istnieje
        struct stat st;
        if (stat(currentPath, &st) == -1) {
            mkdir(currentPath, 0700);  // Uprawnienia dla nowo utworzonego katalogu
            syslog(LOG_INFO, "NEW DIRECTORY SUCCESSFULLY CREATED");

        }
        
        token = strtok(NULL, "/");  // Przechodzimy do następnego katalogu
    }
    
    free(pathCopy);  // Zwolnienie pamięci
}

void copyFile(char* copyFromPath, char* copyToPath){

                int source_fd = open(copyFromPath, O_RDONLY);
                if (source_fd == -1) {
                  perror("Error opening source file");
                  exit(EXIT_FAILURE);
                }
                
                createDirectories(copyToPath, 1);
                int destination_fd = open(copyToPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (destination_fd == -1) {
                  perror("Error opening destination file");
                  close(source_fd);
                  exit(EXIT_FAILURE);
                }

                off_t offset = 0;
                struct stat source_stat;
                if (fstat(source_fd, &source_stat) == -1) {
                    perror("Error getting source file information");
                    close(source_fd);
                    close(destination_fd);
                    exit(EXIT_FAILURE);
                }

                if (sendfile(destination_fd, source_fd, &offset, source_stat.st_size) == -1) {
                    perror("Error copying file");
                    close(source_fd);
                    close(destination_fd);
                    exit(EXIT_FAILURE);
                }
                syslog(LOG_INFO, "File copied successfully.");

                close(source_fd);
                close(destination_fd);
}

