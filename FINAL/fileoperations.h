#ifndef FILEOPERATIONS_H_
#define FILEOPERATIONS_H_

struct FileEntry *createFileEntry(const char *name, const char *path, enum FileType type);

void insertFileEntry(struct FileEntry **head, struct FileEntry *entry);

void calculateMD5(const char *filePath, unsigned char *md5sum);

int compareHashes(const unsigned char *hash1, const unsigned char *hash2);

void traverseDirectory(const char *path, struct FileEntry **head);

void freeLinkedList(struct FileEntry *head);

int isElementInLinkedList(struct FileEntry* element, struct FileEntry* head);

#endif


