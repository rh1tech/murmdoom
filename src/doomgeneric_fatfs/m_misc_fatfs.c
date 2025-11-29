#include "ff.h"
#include "m_misc.h"
#include "z_zone.h"
#include "i_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>

//
// File I/O overrides for FatFs
//

void M_MakeDirectory(char *path)
{
    f_mkdir(path);
}

boolean M_FileExists(char *filename)
{
    FILINFO fno;
    return f_stat(filename, &fno) == FR_OK;
}

long M_FileLength(FILE *handle)
{
    // Not used if w_file_stdc.c is replaced
    return 0;
}

boolean M_WriteFile(char *name, void *source, int length)
{
    FIL file;
    FRESULT fr;
    UINT bw;

    fr = f_open(&file, name, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK) return false;

    fr = f_write(&file, source, length, &bw);
    f_close(&file);

    return (fr == FR_OK && bw == length);
}

int M_ReadFile(char *name, byte **buffer)
{
    FIL file;
    FRESULT fr;
    UINT br;
    int length;
    byte *buf;

    fr = f_open(&file, name, FA_READ);
    if (fr != FR_OK) return 0;

    length = f_size(&file);
    buf = Z_Malloc(length, PU_STATIC, NULL);
    
    fr = f_read(&file, buf, length, &br);
    f_close(&file);

    if (fr != FR_OK || br != length)
    {
        Z_Free(buf);
        return 0;
    }

    *buffer = buf;
    return length;
}

//
// String and Misc functions from m_misc.c
//

// Returns the path to a temporary file of the given name, stored
// inside the system temporary directory.
//
// The returned value must be freed with Z_Free after use.

char *M_TempFile(char *s)
{
    char *tempdir = "/tmp"; // Default for now
    return M_StringJoin(tempdir, DIR_SEPARATOR_S, s, NULL);
}

boolean M_StrToInt(const char *str, int *result)
{
    return sscanf(str, " 0x%x", result) == 1
        || sscanf(str, " 0X%x", result) == 1
        || sscanf(str, " 0%o", result) == 1
        || sscanf(str, " %d", result) == 1;
}

void M_ExtractFileBase(char *path, char *dest)
{
    char *src;
    char *filename;
    int length;

    src = path + strlen(path) - 1;

    // back up until a \ or the start
    while (src != path && *(src - 1) != DIR_SEPARATOR)
    {
	src--;
    }

    filename = src;

    // Copy up to eight characters
    length = 0;
    memset(dest, 0, 8);

    while (*src != '\0' && *src != '.')
    {
        if (length >= 8)
        {
            // printf("Warning: Truncated '%s' lump name to '%.8s'.\n", filename, dest);
            break;
        }

	dest[length++] = toupper((int)*src++);
    }
}

void M_ForceUppercase(char *text)
{
    char *p;

    for (p = text; *p != '\0'; ++p)
    {
        *p = toupper(*p);
    }
}

char *M_StrCaseStr(char *haystack, char *needle)
{
    unsigned int haystack_len;
    unsigned int needle_len;
    unsigned int len;
    unsigned int i;

    haystack_len = strlen(haystack);
    needle_len = strlen(needle);

    if (haystack_len < needle_len)
    {
        return NULL;
    }

    len = haystack_len - needle_len;

    for (i = 0; i <= len; ++i)
    {
        if (!strncasecmp(haystack + i, needle, needle_len))
        {
            return haystack + i;
        }
    }

    return NULL;
}

char *M_StringDuplicate(const char *orig)
{
    char *result;

    result = strdup(orig);

    if (result == NULL)
    {
        I_Error("Failed to duplicate string (length %i)\n",
                strlen(orig));
    }

    return result;
}

char *M_StringReplace(const char *haystack, const char *needle,
                      const char *replacement)
{
    char *result, *dst;
    const char *p;
    size_t needle_len = strlen(needle);
    size_t result_len, dst_len;

    result_len = strlen(haystack) + 1;
    p = haystack;

    for (;;)
    {
        p = strstr(p, needle);
        if (p == NULL)
        {
            break;
        }

        p += needle_len;
        result_len += strlen(replacement) - needle_len;
    }

    result = malloc(result_len);
    if (result == NULL)
    {
        I_Error("M_StringReplace: Failed to allocate new string");
        return NULL;
    }

    dst = result; dst_len = result_len;
    p = haystack;

    while (*p != '\0')
    {
        if (!strncmp(p, needle, needle_len))
        {
            M_StringCopy(dst, replacement, dst_len);
            p += needle_len;
            dst += strlen(replacement);
            dst_len -= strlen(replacement);
        }
        else
        {
            *dst = *p;
            ++dst; --dst_len;
            ++p;
        }
    }

    *dst = '\0';

    return result;
}

boolean M_StringCopy(char *dest, const char *src, size_t dest_size)
{
    size_t len;

    if (dest_size >= 1)
    {
        dest[dest_size - 1] = '\0';
        strncpy(dest, src, dest_size - 1);
    }
    else
    {
        return false;
    }

    len = strlen(dest);
    return src[len] == '\0';
}

boolean M_StringConcat(char *dest, const char *src, size_t dest_size)
{
    size_t offset;

    offset = strlen(dest);
    if (offset > dest_size)
    {
        offset = dest_size;
    }

    return M_StringCopy(dest + offset, src, dest_size - offset);
}

boolean M_StringStartsWith(const char *s, const char *prefix)
{
    return strlen(s) > strlen(prefix)
        && strncmp(s, prefix, strlen(prefix)) == 0;
}

boolean M_StringEndsWith(const char *s, const char *suffix)
{
    return strlen(s) >= strlen(suffix)
        && strcmp(s + strlen(s) - strlen(suffix), suffix) == 0;
}

char *M_StringJoin(const char *s, ...)
{
    char *result;
    const char *v;
    va_list args;
    size_t result_len;

    result_len = strlen(s) + 1;

    va_start(args, s);
    for (;;)
    {
        v = va_arg(args, const char *);
        if (v == NULL)
        {
            break;
        }

        result_len += strlen(v);
    }
    va_end(args);

    result = malloc(result_len);

    if (result == NULL)
    {
        I_Error("M_StringJoin: Failed to allocate new string.");
        return NULL;
    }

    M_StringCopy(result, s, result_len);

    va_start(args, s);
    for (;;)
    {
        v = va_arg(args, const char *);
        if (v == NULL)
        {
            break;
        }

        M_StringConcat(result, v, result_len);
    }
    va_end(args);

    return result;
}

int M_vsnprintf(char *buf, size_t buf_len, const char *s, va_list args)
{
    int result;

    if (buf_len < 1)
    {
        return 0;
    }

    result = vsnprintf(buf, buf_len, s, args);

    if (result < 0 || result >= buf_len)
    {
        buf[buf_len - 1] = '\0';
        result = buf_len - 1;
    }

    return result;
}

int M_snprintf(char *buf, size_t buf_len, const char *s, ...)
{
    va_list args;
    int result;
    va_start(args, s);
    result = M_vsnprintf(buf, buf_len, s, args);
    va_end(args);
    return result;
}
