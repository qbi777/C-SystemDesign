#include <stdio.h>
#include <string.h>
#include "encode.h"
#include "decode.h"
#include "types.h"

static void print_encode_usage(const char *prog)
{
    printf("%s: Encoding: %s -e <.bmp file> <.txt file> [output file]\n", prog, prog);
}

static void print_decode_usage(const char *prog)
{
    printf("%s: Decoding: %s -d <.bmp file> [output file]\n", prog, prog);
}

int main(int argc, char *argv[])
{
    OperationType op;

    /* No arguments : print full usage */
    if (argc < 2)
    {
        print_encode_usage(argv[0]);
        print_decode_usage(argv[0]);
        return 1;
    }

    op = check_operation_type(argv);

    if (op == e_encode)
    {
        EncodeInfo encInfo;
        Status ret;

        if (argc < 4 || argc > 5)
        {
            print_encode_usage(argv[0]);
            return 1;
        }

        memset(&encInfo, 0, sizeof(encInfo));
        if (read_and_validate_encode_args(argv, &encInfo) == e_failure)
        {
            return 1;
        }

        ret = do_encoding(&encInfo);
        close_encode_files(&encInfo);

        if (ret == e_failure)
        {
            fprintf(stderr, "ERROR: Encoding failed\n");
            return 1;
        }
    }
    else if (op == e_decode)
    {
        DecodeInfo decInfo;
        Status ret;

        if (argc < 3 || argc > 4)
        {
            print_decode_usage(argv[0]);
            return 1;
        }

        memset(&decInfo, 0, sizeof(decInfo));
        if (read_and_validate_decode_args(argv, &decInfo) == e_failure)
        {
            return 1;
        }

        ret = do_decoding(&decInfo);
        close_decode_files(&decInfo);

        if (ret == e_failure)
        {
            fprintf(stderr, "ERROR: Decoding failed\n");
            return 1;
        }
    }
    else
    {
        print_encode_usage(argv[0]);
        print_decode_usage(argv[0]);
        return 1;
    }

    return 0;
}
