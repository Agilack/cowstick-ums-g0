/**
 * @file  libc.c
 * @brief Standard library functions
 *
 * @author Saint-Genest Gwenael <gwen@cowlab.fr>
 * @copyright Cowlab (c) 2017-2023
 *
 * @page License
 * This C library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation. You should have received a
 * copy of the GNU Lesser General Public License along with this program,
 * see LICENSE.md file for more details.
 * This program is distributed WITHOUT ANY WARRANTY.
 */
#include "libc.h"
#include "types.h"

/**
 * @brief Copy 'n' bytes from a source buffer to a destination buffer
 *
 * @param dst Pointer to the destination buffer
 * @param src Pointer to the source buffer
 * @param n   NUmber of bytes to copy
 */
void *memcpy(void *dst, const void *src, int n)
{
        u8 *s;
        u8 *d;
        s = (u8*)src;
        d = (u8*)dst;
        while(n)
        {
                *d = *s;
                s ++;
                d ++;
                n --;
        }
        return(dst);
}

/**
 * @brief Fill a buffer with a value (set all bytes)
 *
 * @param dst   Pointer to the buffer to fill
 * @param value Value to set for each bytes
 * @param n     Number of bytes to fill
 */
void *memset(void *dst, int value, int n)
{
	u8 *d;
	d = (u8 *)dst;
	while(n)
	{
		*d  = (u8)value;
		d++;
		n--;
	}
	return(dst);
}

/****************************** STRING functions ******************************/

/**
 * @brief Convert a string of digital characters into an integer value
 *
 * @param s Pointer to the source string
 * @return Integer value of the converted string
 */
int atoi(char *s)
{
	int  result;
	uint len;
	int  v;
	uint i;

	/* Get the number of digit */
	len = strlen(s);

	/* Get highest 10's power factor */
	v = 1;
	for (i = 1; i < len; i++)
		v = v * 10;

	/* Compute the value */
	result = 0;
	for (i = 0; i < len; i++)
	{
		result = result + ((*s - '0') * v);
		s++;
		v = v / 10;
	}

	return result;
}

/**
 * @brief Generic function to convert a 24bits value to network byte order
 *
 * @param v Long integer with bytes in CPU order
 * @return  Long integer with bytes in network order
 */
u32 hton3(u32 v)
{
	u32 vout;
	vout  = ((v & 0x0000FF) << 16);
	vout |= ((v & 0x00FF00)      );
	vout |= ((v & 0xFF0000) >> 16);
	return(vout);
}

/**
 * @brief Generic function to convert a long value to network byte order
 *
 * @param v Long integer with bytes in CPU order
 * @return  Long integer with bytes in network order
 */
u32 htonl(u32 v)
{
	u32 vout;
	vout  = ((v & 0x000000FF) << 24);
	vout |= ((v & 0x0000FF00) <<  8);
	vout |= ((v & 0x00FF0000) >>  8);
	vout |= ((v & 0xFF000000) >> 24);
	return(vout);
}

/**
 * @brief Generic function to convert a short value to network byte order
 *
 * @param v Short integer with bytes in CPU order
 * @return  Short integer with bytes in network order
 */
u16 htons(u16 v)
{
	u16 vout;
	vout = (u16)((v & 0xFF) << 8) | (v >> 8);
	return(vout);
}

/**
 * @brief Convert an integer value to his decimal representation into an ASCII string
 *
 * @param d Pointer to a buffer for output string
 * @param n Interger value to convert
 * @param pad Align output value to at least "pad" digits
 * @param zero Boolean flag, if set a \0 is added at the end of string
 */
int itoa(char *d, uint n, int pad, int zero)
{
	unsigned int decade = 1000000000;
	int count = 0;
	int i;

	for (i = 0; i < 9; i++)
	{
		if ((n > (decade - 1)) || count || (pad >= (10-i)))
		{
			*d = (u8)(n / decade) + '0';
			n -= ((n / decade) * decade);
			d++;
			count++;
		}
		decade = (decade / 10);
	}
	*d = (u8)(n + '0');
	count ++;

	if (zero)
		d[1] = 0;

	return(count);
}

/**
 * @brief Append a string to the end of a string
 *
 * @param dest Pointer to the destination string (where data are append)
 * @param src  Pointer to the source string
 * @return Pointer to the destination string
 */
char * strcat (char *dest, const char *src)
{
	return strncat(dest, src, strlen(src));
}

/**
 * @brief Search the first occurence of a character into a string
 *
 * @param s Pointer to the string where character is searched
 * @param c Character to search
 * @return Pointer to the first occurrence of the character (or NULL if not found)
 */
char *strchr(const char *s, int c)
{
        char *ptr;

        for (ptr = (char *)s; ptr != 0; ptr++)
        {
                if (*ptr == 0)
                        break;
                if (*ptr == c)
                        return ptr;
        }
        return 0;
}

/**
 * @brief Copy a string to another buffer
 *
 * @param dest Pointer to the destination buffer
 * @param src  Pointer to the string to copy (source). Must be NULL terminated
 * @return Pointer to the destination buffer
 */
char *strcpy(char *dest, const char *src)
{
	return strncpy(dest, src, strlen(src));
}

/**
 * @brief Count the number of character into a string
 *
 * @param Pointer to the string (NULL terminated)
 * @return Number of character into the string
 */
uint strlen(const char *str)
{
	uint count;
	count = 0;
	while(*str)
	{
		count ++;
		str++;
	}
	return(count);
}

/**
 * @brief Append a string to the end of a string, with a length limit
 *
 * @param dest Pointer to the destination string (where data are append)
 * @param src  Pointer to the source string
 * @param len  Maximum number of bytes to copy
 * @return Pointer to the destination string
 */
char * strncat (char *dest, const char *src, uint len)
{
	char *s1 = dest;
	const char *s2 = src;
	char c;
	/* Find the end of the string. */
	do
		c = *s1++;
	while (c != '\0');
	/* Make S1 point before the next character, so we can increment
	it while memory is read (wins on pipelined cpus). */
	s1 -= 2;
	do
	{
		c = *s2++;
		*++s1 = c;
		len--;
	}
	while ((c != '\0') && (len > 0));

	if (len == 0)
	        *++s1 = 0;

	return dest;
}

/**
 * @brief Compare two strings, with a maximum number of bytes
 *
 * @param p1 Pointer to the first string to compare
 * @param p2 Pointer to the second string to compare
 * @param len Maximum number of characters to compare
 * @return Zero if strings are equal, a non-nul value if strings differs
 */
int strncmp(const char *p1, const char *p2, uint len)
{
	register const unsigned char *s1 = (const unsigned char *) p1;
	register const unsigned char *s2 = (const unsigned char *) p2;
	unsigned char c1, c2;

	do
	{
		if (len == 0)
			return 0;

		c1 = (unsigned char) *s1++;
		c2 = (unsigned char) *s2++;
		if (c1 == '\0')
			return c1 - c2;
                len --;
	}
	while (c1 == c2);
	return c1 - c2;
}

/**
 * @brief Copy a string to another buffer, with a limit of 'len' bytes
 *
 * @param dest Pointer to the destination buffer
 * @param src  Pointer to the string to copy (source). Must be NULL terminated
 * @param len  Maximum number of character to copy
 * @return Number of character into the string
 */
char *strncpy(char *dest, const char *src, uint len)
{
	char *dsave = dest;
	while ( (*src) && (len > 0))
	{
		*dest = *src;
		src++;
		dest++;
		len--;
	}
	*dest = 0;
	return dsave;
}
/* EOF */
