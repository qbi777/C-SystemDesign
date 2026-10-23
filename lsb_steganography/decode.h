#ifndef DECODE_H
#define DECODE_H

#include <stdio.h>
#include "types.h" // Contains user defined types
#include "common.h"

/*
 * Structure to store information required for
 * decoding the secret file from the stego image
 */

#define MAX_DECODE_FNAME 256

typedef struct _DecodeInfo
{
    /* Stego Image info */
    char *stego_image_fname;
    FILE *fptr_stego_image;

    /* Output (decoded) file info */
    char *output_fname_arg;                 /* name given by user, or NULL */
    char output_fname[MAX_DECODE_FNAME];    /* final name used */
    FILE *fptr_output;

    /* Data decoded from the image */
    char extn_output_file[MAX_FILE_SUFFIX + 1];
    uint extn_size;
    uint size_output_file;

} DecodeInfo;


/* Decoding function prototypes */

/* Read and validate Decode args from argv */
Status read_and_validate_decode_args(char *argv[], DecodeInfo *decInfo);

/* Perform the decoding */
Status do_decoding(DecodeInfo *decInfo);

/* Open the stego image */
Status open_stego_file(DecodeInfo *decInfo);

/* Open the output file (name decided after the extension is known) */
Status open_output_file(DecodeInfo *decInfo);

/* Close all files opened for decoding */
void close_decode_files(DecodeInfo *decInfo);

/* Decode and verify the magic string */
Status decode_magic_string(DecodeInfo *decInfo);

/* Decode the extension size and the extension */
Status decode_secret_file_extn(DecodeInfo *decInfo);

/* Decode the secret file size */
Status decode_secret_file_size(DecodeInfo *decInfo);

/* Decode the secret file data and write it to the output file */
Status decode_secret_file_data(DecodeInfo *decInfo);

/* Decode 'size' bytes from the image into 'data' */
Status decode_data_from_image(int size, FILE *fptr_stego_image, char *data);

/* Decode a byte from the LSBs of 8 image bytes */
char decode_byte_from_lsb(const char *image_buffer);

/* Decode a 32 bit number from the LSBs of 32 image bytes */
uint decode_size_from_lsb(const char *image_buffer);

#endif
