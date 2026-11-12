/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 PolarPatch */
/* <string.h> for the engine on VIBE FM: the functions the VIBE FM SDK provides (sdk/src/vibe_sdk.c), nothing more
 * (freestanding WebAssembly has no libc; sdk/tools/build.sh puts this folder on the include path) */
#ifndef ASV_STRING_H
#define ASV_STRING_H
#include <stddef.h>
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
void *memchr(const void *s, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strcpy(char *d, const char *s);
char *strncpy(char *d, const char *s, size_t n);
char *strchr(const char *s, int c);
#endif
