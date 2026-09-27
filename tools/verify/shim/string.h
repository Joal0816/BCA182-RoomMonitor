#ifndef _SHIM_STRING_H
#define _SHIM_STRING_H
int strcmp(const char*, const char*);
size_t strlen(const char*);
void *memset(void*, int, size_t);
void *memcpy(void*, const void*, size_t);
#endif
