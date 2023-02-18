/**
 * @file  libc.h
 * @brief Definitions and prototypes for standard library functions
 *
 * @author Saint-Genest Gwenael <gwen@cowlab.fr>
 * @copyright Agilack (c) 2017-2023
 *
 * @page License
 * This C library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation. You should have received a
 * copy of the GNU Lesser General Public License along with this program,
 * see LICENSE.md file for more details.
 * This program is distributed WITHOUT ANY WARRANTY.
 */
#ifndef LIBC_H
#define LIBC_H
#include "types.h"

void *memcpy (void *dst, const void *src, int n);
void *memset (void *dst, int value, int n);

int   atoi(char *s);
unsigned long  htonl(unsigned long  v);
unsigned short htons(unsigned short v);
int   itoa(char *d, uint n, int pad, int zero);
char *strcat (char *dest, const char *src);
char *strchr (const char *s, int c);
char *strcpy (char *dest, const char *src);
uint  strlen (const char *str);
char *strncat(char *dest, const char *src, uint len);
int   strncmp(const char *p1, const char *p2, uint len);
char *strncpy(char *dest, const char *src, uint len);

#endif
