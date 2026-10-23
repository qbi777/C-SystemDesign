#include <stdio.h>
#include <string.h>
#include "encode.h"
#include "types.h"
#include "common.h"

/* Function Definitions */

/* Get image size
 * Input: Image file ptr
 * Output: width * height * bytes per pixel (3 in our case)
 * Description: In BMP Image, width is stored in offset 18,
 * and height after that. size is 4 bytes
 */
uint get_image_size_for_bmp(FILE *fptr_image)
{
    int width = 0, height = 0;

    // Seek to 18th byte
    fseek(fptr_image, 18, SEEK_SET);

    // Read the width (an int) and the height (an int)
    if (fread(&width, sizeof(int), 1, fptr_image) != 1 ||
        fread(&height, sizeof(int), 1, fptr_image) != 1)
    {
        return 0;
    }

    /* A negative height just means a top-down bitmap */
    if (width < 0)  width = -width;
    if (height < 0) height = -height;

    // Return image capacity
    return (uint)width * (uint)height * 3;
}

/* Get file size
 * Input: file ptr
 * Output: size of the file in bytes
 */
uint get_file_size(FILE *fptr)
{
    long size;

    fseek(fptr, 0, SEEK_END);
    size = ftell(fptr);
    rewind(fptr);

    return (size < 0) ? 0 : (uint)size;
}

/* Check the operation type from argv[1] */
OperationType check_operation_type(char *argv[])
{
    if (argv[1] == NULL)
    {
        return e_unsupported;
    }
    if (strcmp(argv[1], "-e") == 0)
    {
        return e_encode;
    }
    if (strcmp(argv[1], "-d") == 0)
    {
        return e_decode;
    }
    return e_unsupported;
}

/*
 * Read and validate the encode arguments
 * ./lsb_steg -e <.bmp file> <secret file> [output file]
 */
Status read_and_validate_encode_args(char *argv[], EncodeInfo *encInfo)
{
    const char *extn;

    /* argv[2] : source image, must be a .bmp */
    if (argv[2] == NULL || !has_bmp_extension(argv[2]))
    {
        fprintf(stderr, "ERROR: Source image must be a .bmp file\n");
        return e_failure;
    }
    encInfo->src_image_fname = argv[2];

    /* argv[3] : secret file, must be present and have an extension */
    if (argv[3] == NULL)
    {
        fprintf(stderr, "ERROR: Secret file is not provided\n");
        return e_failure;
    }
    extn = get_file_extension(argv[3]);
    if (extn == NULL)
    {
        fprintf(stderr, "ERROR: Secret file \"%s\" must have a file extension (e.g. .txt)\n", argv[3]);
        return e_failure;
    }
    if (strlen(extn) > MAX_FILE_SUFFIX)
    {
        fprintf(stderr, "ERROR: File extension \"%s\" is too long (max %d characters)\n", extn, MAX_FILE_SUFFIX);
        return e_failure;
    }
    encInfo->secret_fname = argv[3];
    strcpy(encInfo->extn_secret_file, extn);

    /* argv[4] : optional output file */
    if (argv[4] != NULL)
    {
        if (!has_bmp_extension(argv[4]))
        {
            fprintf(stderr, "ERROR: Output image must be a .bmp file\n");
            return e_failure;
        }
        encInfo->stego_image_fname = argv[4];
    }
    else
    {
        printf("INFO: Output File not mentioned. Creating %s as default\n", DEFAULT_STEGO_FNAME);
        encInfo->stego_image_fname = DEFAULT_STEGO_FNAME;
    }

    /* Source image and output image must not be the same file */
    if (strcmp(encInfo->src_image_fname, encInfo->stego_image_fname) == 0)
    {
        fprintf(stderr, "ERROR: Source and output image cannot be the same file\n");
        return e_failure;
    }

    return e_success;
}

/* 
 * Get File pointers for i/p and o/p files
 * Inputs: Src Image file, Secret file and
 * Stego Image file
 * Output: FILE pointer for above files
 * Return Value: e_success or e_failure, on file errors
 */
Status open_files(EncodeInfo *encInfo)
{
    // Src Image file
    encInfo->fptr_src_image = fopen(encInfo->src_image_fname, "rb");
    // Do Error handling
    if (encInfo->fptr_src_image == NULL)
    {
    	perror("fopen");
    	fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->src_image_fname);

    	return e_failure;
    }
    printf("INFO: Opened %s\n", encInfo->src_image_fname);

    // Secret file
    encInfo->fptr_secret = fopen(encInfo->secret_fname, "rb");
    // Do Error handling
    if (encInfo->fptr_secret == NULL)
    {
    	perror("fopen");
    	fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->secret_fname);

    	return e_failure;
    }
    printf("INFO: Opened %s\n", encInfo->secret_fname);

    // Stego Image file
    encInfo->fptr_stego_image = fopen(encInfo->stego_image_fname, "wb");
    // Do Error handling
    if (encInfo->fptr_stego_image == NULL)
    {
    	perror("fopen");
    	fprintf(stderr, "ERROR: Unable to open file %s\n", encInfo->stego_image_fname);

    	return e_failure;
    }
    printf("INFO: Opened %s\n", encInfo->stego_image_fname);

    // No failure return e_success
    return e_success;
}

/* Close every file that was opened */
void close_encode_files(EncodeInfo *encInfo)
{
    if (encInfo->fptr_src_image)    { fclose(encInfo->fptr_src_image);    encInfo->fptr_src_image = NULL; }
    if (encInfo->fptr_secret)       { fclose(encInfo->fptr_secret);       encInfo->fptr_secret = NULL; }
    if (encInfo->fptr_stego_image)  { fclose(encInfo->fptr_stego_image);  encInfo->fptr_stego_image = NULL; }
}

/* Verify that the source image is an uncompressed 24 bit BMP */
static Status check_bmp_format(FILE *fptr)
{
    char sig[2];
    unsigned short bpp = 0;
    uint compression = 0;

    rewind(fptr);
    if (fread(sig, 1, 2, fptr) != 2 || sig[0] != 'B' || sig[1] != 'M')
    {
        fprintf(stderr, "ERROR: Source file is not a valid BMP image\n");
        return e_failure;
    }

    fseek(fptr, 28, SEEK_SET);
    if (fread(&bpp, sizeof(bpp), 1, fptr) != 1 || bpp != 24)
    {
        fprintf(stderr, "ERROR: Only 24 bit BMP images are supported\n");
        return e_failure;
    }

    fseek(fptr, 30, SEEK_SET);
    if (fread(&compression, sizeof(compression), 1, fptr) != 1 || compression != 0)
    {
        fprintf(stderr, "ERROR: Compressed BMP images are not supported\n");
        return e_failure;
    }

    return e_success;
}

/*
 * Check whether the image can hold:
 * magic string + extn size (4) + extn + file size (4) + file data
 * Every byte needs 8 image bytes.
 */
Status check_capacity(EncodeInfo *encInfo)
{
    unsigned long long bits_needed;

    printf("INFO: Checking for %s size\n", encInfo->secret_fname);
    encInfo->size_secret_file = get_file_size(encInfo->fptr_secret);
    if (encInfo->size_secret_file == 0)
    {
        fprintf(stderr, "ERROR: \"%s\" is empty, nothing to encode\n", encInfo->secret_fname);
        return e_failure;
    }
    printf("INFO: Done. Not Empty\n");

    printf("INFO: Checking for %s capacity to handle %s\n", encInfo->src_image_fname, encInfo->secret_fname);
    if (check_bmp_format(encInfo->fptr_src_image) == e_failure)
    {
        return e_failure;
    }

    encInfo->image_capacity = get_image_size_for_bmp(encInfo->fptr_src_image);

    bits_needed = ((unsigned long long)strlen(MAGIC_STRING)
                  + sizeof(uint) + strlen(encInfo->extn_secret_file)
                  + sizeof(uint) + (unsigned long long)encInfo->size_secret_file) * 8ULL;

    if (encInfo->image_capacity < bits_needed)
    {
        fprintf(stderr, "ERROR: \"%s\" doesn't have the capacity to encode \"%s\"\n",
                encInfo->src_image_fname, encInfo->secret_fname);
        return e_failure;
    }
    printf("INFO: Done. Found OK\n");
    return e_success;
}

/* Copy the 54 byte BMP header as it is */
Status copy_bmp_header(FILE *fptr_src_image, FILE *fptr_dest_image)
{
    char header[BMP_HEADER_SIZE];

    rewind(fptr_src_image);
    if (fread(header, 1, BMP_HEADER_SIZE, fptr_src_image) != BMP_HEADER_SIZE)
    {
        fprintf(stderr, "ERROR: Unable to read BMP header\n");
        return e_failure;
    }
    if (fwrite(header, 1, BMP_HEADER_SIZE, fptr_dest_image) != BMP_HEADER_SIZE)
    {
        fprintf(stderr, "ERROR: Unable to write BMP header\n");
        return e_failure;
    }
    return e_success;
}

/* Hide the 8 bits of 'data' (MSB first) in the LSBs of 8 image bytes */
Status encode_byte_to_lsb(char data, char *image_buffer)
{
    int i;

    for (i = 0; i < 8; i++)
    {
        image_buffer[i] = (char)((image_buffer[i] & 0xFE) | ((data >> (7 - i)) & 1));
    }
    return e_success;
}

/* Hide a 32 bit number (MSB first) in the LSBs of 32 image bytes */
Status encode_size_to_lsb(uint size, char *image_buffer)
{
    int i;

    for (i = 0; i < 32; i++)
    {
        image_buffer[i] = (char)((image_buffer[i] & 0xFE) | ((size >> (31 - i)) & 1));
    }
    return e_success;
}

/* Encode 'size' bytes of 'data' : read 8 image bytes per data byte, modify, write */
Status encode_data_to_image(char *data, int size, FILE *fptr_src_image, FILE *fptr_stego_image)
{
    char image_buffer[MAX_IMAGE_BUF_SIZE];
    int i;

    for (i = 0; i < size; i++)
    {
        if (fread(image_buffer, 1, 8, fptr_src_image) != 8)
        {
            fprintf(stderr, "ERROR: Unexpected end of image data\n");
            return e_failure;
        }
        encode_byte_to_lsb(data[i], image_buffer);
        if (fwrite(image_buffer, 1, 8, fptr_stego_image) != 8)
        {
            fprintf(stderr, "ERROR: Unable to write to stego image\n");
            return e_failure;
        }
    }
    return e_success;
}

/* Encode the magic string */
Status encode_magic_string(const char *magic_string, EncodeInfo *encInfo)
{
    return encode_data_to_image((char *)magic_string, (int)strlen(magic_string),
                                encInfo->fptr_src_image, encInfo->fptr_stego_image);
}

/* Encode a 32 bit number using 32 image bytes */
static Status encode_uint(uint value, EncodeInfo *encInfo)
{
    char image_buffer[32];

    if (fread(image_buffer, 1, 32, encInfo->fptr_src_image) != 32)
    {
        fprintf(stderr, "ERROR: Unexpected end of image data\n");
        return e_failure;
    }
    encode_size_to_lsb(value, image_buffer);
    if (fwrite(image_buffer, 1, 32, encInfo->fptr_stego_image) != 32)
    {
        fprintf(stderr, "ERROR: Unable to write to stego image\n");
        return e_failure;
    }
    return e_success;
}

/* Encode the length of the extension */
Status encode_secret_file_extn_size(uint extn_size, EncodeInfo *encInfo)
{
    return encode_uint(extn_size, encInfo);
}

/* Encode the extension (e.g. ".txt") */
Status encode_secret_file_extn(const char *file_extn, EncodeInfo *encInfo)
{
    return encode_data_to_image((char *)file_extn, (int)strlen(file_extn),
                                encInfo->fptr_src_image, encInfo->fptr_stego_image);
}

/* Encode the size of the secret file */
Status encode_secret_file_size(long file_size, EncodeInfo *encInfo)
{
    return encode_uint((uint)file_size, encInfo);
}

/* Encode the secret file contents */
Status encode_secret_file_data(EncodeInfo *encInfo)
{
    long count = 0;

    rewind(encInfo->fptr_secret);
    while (fread(encInfo->secret_data, 1, MAX_SECRET_BUF_SIZE, encInfo->fptr_secret) == MAX_SECRET_BUF_SIZE)
    {
        if (encode_data_to_image(encInfo->secret_data, MAX_SECRET_BUF_SIZE,
                                 encInfo->fptr_src_image, encInfo->fptr_stego_image) == e_failure)
        {
            return e_failure;
        }
        count++;
    }

    if (count != encInfo->size_secret_file)
    {
        fprintf(stderr, "ERROR: Could not read the complete secret file\n");
        return e_failure;
    }
    return e_success;
}

/* Copy whatever is left of the source image to the stego image */
Status copy_remaining_img_data(FILE *fptr_src, FILE *fptr_dest)
{
    char buffer[4096];
    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), fptr_src)) > 0)
    {
        if (fwrite(buffer, 1, n, fptr_dest) != n)
        {
            fprintf(stderr, "ERROR: Unable to write to stego image\n");
            return e_failure;
        }
    }
    return e_success;
}

/* Perform the whole encoding */
Status do_encoding(EncodeInfo *encInfo)
{
    printf("INFO: Opening required files\n");
    if (open_files(encInfo) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done\n");

    printf("INFO: ## Encoding Procedure Started ##\n");

    if (check_capacity(encInfo) == e_failure)
    {
        return e_failure;
    }

    printf("INFO: Copying Image Header\n");
    if (copy_bmp_header(encInfo->fptr_src_image, encInfo->fptr_stego_image) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done\n");

    printf("INFO: Encoding Magic String Signature\n");
    if (encode_magic_string(MAGIC_STRING, encInfo) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done\n");

    printf("INFO: Encoding %s File Extension\n", encInfo->secret_fname);
    if (encode_secret_file_extn_size((uint)strlen(encInfo->extn_secret_file), encInfo) == e_failure ||
        encode_secret_file_extn(encInfo->extn_secret_file, encInfo) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done\n");

    printf("INFO: Encoding %s File Size\n", encInfo->secret_fname);
    if (encode_secret_file_size(encInfo->size_secret_file, encInfo) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done\n");

    printf("INFO: Encoding %s File Data\n", encInfo->secret_fname);
    if (encode_secret_file_data(encInfo) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done\n");

    printf("INFO: Copying Left Over Data\n");
    if (copy_remaining_img_data(encInfo->fptr_src_image, encInfo->fptr_stego_image) == e_failure)
    {
        return e_failure;
    }
    printf("INFO: Done\n");

    printf("INFO: ## Encoding Done Successfully ##\n");
    return e_success;
}
