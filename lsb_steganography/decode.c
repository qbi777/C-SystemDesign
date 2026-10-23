#include <stdio.h>
#include <string.h>
#include "decode.h"
#include "types.h"
#include "common.h"

/*
 * Read and validate the decode arguments
 * ./lsb_steg -d <.bmp file> [output file]
 */
Status read_and_validate_decode_args(char *argv[], DecodeInfo *decInfo)
{
    if (argv[2] == NULL || !has_bmp_extension(argv[2]))
    {
        fprintf(stderr, "ERROR: Stego image must be a .bmp file\n");
        return e_failure;
    }
    decInfo->stego_image_fname = argv[2];
    decInfo->output_fname_arg = argv[3];   /* NULL if not given */

    return e_success;
}

/* Open the stego image for reading */
Status open_stego_file(DecodeInfo *decInfo)
{
    decInfo->fptr_stego_image = fopen(decInfo->stego_image_fname, "rb");
    if (decInfo->fptr_stego_image == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", decInfo->stego_image_fname);
        return e_failure;
    }
    printf("INFO: Opened %s\n", decInfo->stego_image_fname);
    return e_success;
}

/*
 * Decide the output file name and open it.
 *  - not given          -> decoded<extn>
 *  - given w/o extension -> <name><extn>
 *  - given with extension -> used as it is
 */
Status open_output_file(DecodeInfo *decInfo)
{
    int n;

    if (decInfo->output_fname_arg == NULL)
    {
        n = snprintf(decInfo->output_fname, MAX_DECODE_FNAME, "%s%s",
                     DEFAULT_DECODED_NAME, decInfo->extn_output_file);
        printf("INFO: Output File not mentioned. Creating %s as default\n", decInfo->output_fname);
    }
    else if (get_file_extension(decInfo->output_fname_arg) == NULL)
    {
        n = snprintf(decInfo->output_fname, MAX_DECODE_FNAME, "%s%s",
                     decInfo->output_fname_arg, decInfo->extn_output_file);
    }
    else
    {
        n = snprintf(decInfo->output_fname, MAX_DECODE_FNAME, "%s", decInfo->output_fname_arg);
    }

    if (n < 0 || n >= MAX_DECODE_FNAME)
    {
        fprintf(stderr, "ERROR: Output file name is too long\n");
        return e_failure;
    }

    decInfo->fptr_output = fopen(decInfo->output_fname, "wb");
    if (decInfo->fptr_output == NULL)
    {
        perror("fopen");
        fprintf(stderr, "ERROR: Unable to open file %s\n", decInfo->output_fname);
        return e_failure;
    }
    printf("INFO: Opened %s\n", decInfo->output_fname);
    return e_success;
}

void close_decode_files(DecodeInfo *decInfo)
{
    if (decInfo->fptr_stego_image) { fclose(decInfo->fptr_stego_image); decInfo->fptr_stego_image = NULL; }
    if (decInfo->fptr_output)      { fclose(decInfo->fptr_output);      decInfo->fptr_output = NULL; }
}

/* Rebuild a byte (MSB first) from the LSBs of 8 image bytes */
char decode_byte_from_lsb(const char *image_buffer)
{
    int i;
    unsigned char data = 0;

    for (i = 0; i < 8; i++)
    {
        data = (unsigned char)((data << 1) | (image_buffer[i] & 1));
    }
    return (char)data;
}

/* Rebuild a 32 bit number (MSB first) from the LSBs of 32 image bytes */
uint decode_size_from_lsb(const char *image_buffer)
{
    int i;
    uint size = 0;

    for (i = 0; i < 32; i++)
    {
        size = (size << 1) | (uint)(image_buffer[i] & 1);
    }
    return size;
}

/* Decode 'size' bytes from the image */
Status decode_data_from_image(int size, FILE *fptr_stego_image, char *data)
{
    char image_buffer[8];
    int i;

    for (i = 0; i < size; i++)
    {
        if (fread(image_buffer, 1, 8, fptr_stego_image) != 8)
        {
            fprintf(stderr, "ERROR: Unexpected end of image data\n");
            return e_failure;
        }
        data[i] = decode_byte_from_lsb(image_buffer);
    }
    return e_success;
}

/* Read a 32 bit number from the image */
static Status decode_uint(FILE *fptr, uint *value)
{
    char image_buffer[32];

    if (fread(image_buffer, 1, 32, fptr) != 32)
    {
        fprintf(stderr, "ERROR: Unexpected end of image data\n");
        return e_failure;
    }
    *value = decode_size_from_lsb(image_buffer);
    return e_success;
}

/* Skip the header and check the magic string */
Status decode_magic_string(DecodeInfo *decInfo)
{
    char magic[sizeof(MAGIC_STRING)];
    size_t len = strlen(MAGIC_STRING);

    fseek(decInfo->fptr_stego_image, BMP_HEADER_SIZE, SEEK_SET);

    if (decode_data_from_image((int)len, decInfo->fptr_stego_image, magic) == e_failure)
    {
        return e_failure;
    }
    magic[len] = '\0';

    if (strcmp(magic, MAGIC_STRING) != 0)
    {
        fprintf(stderr, "ERROR: Magic string not found. \"%s\" does not contain any encoded data\n",
                decInfo->stego_image_fname);
        return e_failure;
    }
    return e_success;
}

/* Decode extension size, then the extension */
Status decode_secret_file_extn(DecodeInfo *decInfo)
{
    if (decode_uint(decInfo->fptr_stego_image, &decInfo->extn_size) == e_failure)
    {
        return e_failure;
    }

    if (decInfo->extn_size == 0 || decInfo->extn_size > MAX_FILE_SUFFIX)
    {
        fprintf(stderr, "ERROR: Invalid extension size found. Image data is corrupted\n");
        return e_failure;
    }

    if (decode_data_from_image((int)decInfo->extn_size, decInfo->fptr_stego_image,
                               decInfo->extn_output_file) == e_failure)
    {
        return e_failure;
    }
    decInfo->extn_output_file[decInfo->extn_size] = '\0';
    return e_success;
}

/* Decode the size of the hidden file and sanity check it */
Status decode_secret_file_size(DecodeInfo *decInfo)
{
    long cur, end;
    unsigned long long available;

    if (decode_uint(decInfo->fptr_stego_image, &decInfo->size_output_file) == e_failure)
    {
        return e_failure;
    }

    cur = ftell(decInfo->fptr_stego_image);
    fseek(decInfo->fptr_stego_image, 0, SEEK_END);
    end = ftell(decInfo->fptr_stego_image);
    fseek(decInfo->fptr_stego_image, cur, SEEK_SET);

    available = (end > cur) ? (unsigned long long)(end - cur) / 8ULL : 0ULL;
    if (decInfo->size_output_file > available)
    {
        fprintf(stderr, "ERROR: Encoded file size is larger than the image can hold. Image data is corrupted\n");
        return e_failure;
    }
    return e_success;
}

/* Decode the hidden file contents */
Status decode_secret_file_data(DecodeInfo *decInfo)
{
    char ch;
    uint i;

    for (i = 0; i < decInfo->size_output_file; i++)
    {
        if (decode_data_from_image(1, decInfo->fptr_stego_image, &ch) == e_failure)
        {
            return e_failure;
        }
        if (fputc((unsigned char)ch, decInfo->fptr_output) == EOF)
        {
            fprintf(stderr, "ERROR: Unable to write to %s\n", decInfo->output_fname);
            return e_failure;
        }
    }
    return e_success;
}

/* Perform the whole decoding */
Status do_decoding(DecodeInfo *decInfo)
{
    printf("INFO: ## Decoding Procedure Started ##\n");

    printf("INFO: Opening required files\n");
    if (open_stego_file(decInfo) == e_failure)
    {
        return e_failure;
    }

    printf("INFO: Decoding Magic String Signature\n");
    if (decode_magic_string(decInfo) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done\n");

    printf("INFO: Decoding Output File Extension\n");
    if (decode_secret_file_extn(decInfo) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done\n");

    if (open_output_file(decInfo) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done. Opened all required files\n");

    printf("INFO: Decoding %s File Size\n", decInfo->output_fname);
    if (decode_secret_file_size(decInfo) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done\n");

    printf("INFO: Decoding %s File Data\n", decInfo->output_fname);
    if (decode_secret_file_data(decInfo) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done\n");

    printf("INFO: ## Decoding Done Successfully ##\n");
    return e_success;
}
