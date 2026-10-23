#include <stdio.h>
#include "encode.h"
#include "types.h"

/* Small test driver for open_files() and get_image_size_for_bmp() */
int main()
{
    EncodeInfo encInfo;
    uint img_size;

    // Fill with sample filenames
    encInfo.src_image_fname = "beautiful.bmp";
    encInfo.secret_fname = "secret.txt";
    encInfo.stego_image_fname = "stego_img.bmp";
    encInfo.fptr_src_image = encInfo.fptr_secret = encInfo.fptr_stego_image = NULL;

    // Test open_files
    if (open_files(&encInfo) == e_failure)
    {
    	printf("ERROR: %s function failed\n", "open_files" );
    	close_encode_files(&encInfo);
    	return 1;
    }
    else
    {
    	printf("SUCCESS: %s function completed\n", "open_files" );
    }

    // Test get_image_size_for_bmp
    img_size = get_image_size_for_bmp(encInfo.fptr_src_image);
    printf("INFO: Image size = %u\n", img_size);

    close_encode_files(&encInfo);
    return 0;
}
