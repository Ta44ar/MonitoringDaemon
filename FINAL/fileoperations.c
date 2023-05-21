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
        snprintf(filePath, sizeof(filePath), "%s/%s", path, entry->d_name);

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

