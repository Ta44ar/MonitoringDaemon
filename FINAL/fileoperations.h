#ifndef FILEOPERATIONS_H_
#define FILEOPERATIONS_H_

extern int recursive;

enum FileType {
    FILE_TYPE,
    DIRECTORY_TYPE
};

struct FileEntry {
    char name[256];
    char path[1024];
    unsigned char md5sum[MD5_DIGEST_LENGTH];
    enum FileType type;
    struct FileEntry *next;
};

struct FileEntry *createFileEntry(const char *name, const char *path, enum FileType type);

void insertFileEntry(struct FileEntry **head, struct FileEntry *entry);

void calculateMD5(const char *filePath, unsigned char *md5sum);

int compareHashes(const unsigned char *hash1, const unsigned char *hash2);

void traverseDirectory(const char *path, struct FileEntry **head);

void freeLinkedList(struct FileEntry *head);

int isElementInLinkedList(struct FileEntry* element, struct FileEntry* head);

char* absoluteToRelative(const char* absolutePath, const char* currentFolder);

void getDestinationFilePath(char* temp_path, char* destination_path, char* current_path, char* source_path);

void createDirectories(char* path);

void copyFile(char* copyFromPath, char* copyToPath);

#endif


