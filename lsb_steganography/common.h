#ifndef COMMON_H
#define COMMON_H

#include <string.h>
#include <ctype.h>

/* Magic string to identify whether stegged or not */
#define MAGIC_STRING "#*"

/* Size of the BMP header (file header + info header) in bytes */
#define BMP_HEADER_SIZE 54

/* Max length of a file extension including the dot, e.g. ".txt" */
#define MAX_FILE_SUFFIX 8

/* Default file names */
#define DEFAULT_STEGO_FNAME  "steged_img.bmp"
#define DEFAULT_DECODED_NAME "decoded"

/*
 * Returns pointer to the extension (including '.') of a file name,
 * or NULL if the name has no usable extension.
 */
static inline const char *get_file_extension(const char *fname)
{
    const char *dot = strrchr(fname, '.');

    if (dot == NULL || dot == fname || dot[1] == '\0' || strchr(dot, '/') != NULL)
    {
        return NULL;
    }
    return dot;
}

/* Returns 1 if the file name ends with ".bmp" (case insensitive) */
static inline int has_bmp_extension(const char *fname)
{
    const char *ext = get_file_extension(fname);
    const char *want = ".bmp";
    size_t i;

    if (ext == NULL || strlen(ext) != 4)
    {
        return 0;
    }
    for (i = 0; i < 4; i++)
    {
        if (tolower((unsigned char)ext[i]) != want[i])
        {
            return 0;
        }
    }
    return 1;
}

#endif
