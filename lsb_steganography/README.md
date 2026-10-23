# LSB Image Steganography (C)

Hide any small file (text, source code, etc.) inside a 24-bit `.bmp` image by
replacing the **least significant bit** of the image bytes, and recover it again.
The image looks identical to the eye (each byte changes by at most 1).

## Files

| File | Purpose |
|------|---------|
| `main.c` | Command line handling (`-e` / `-d`), usage messages |
| `encode.c / encode.h` | Argument validation, capacity check, encoding |
| `decode.c / decode.h` | Argument validation, decoding |
| `common.h` | Magic string, shared constants, small helpers |
| `types.h` | `Status`, `OperationType`, `uint` |
| `test_encode.c` | Small test driver for `open_files()` / `get_image_size_for_bmp()` |
| `Makefile` | Build rules |
| `beautiful.bmp`, `secret.txt` | Sample input files |

## Build

```bash
make            # builds ./lsb_steg
make test_encode  # optional: builds the small test driver
make clean
```

Needs `gcc` and `make` only.

## Usage

```
./lsb_steg -e <.bmp file> <secret file> [output .bmp]
./lsb_steg -d <.bmp file> [output file]
```

### Encode
```bash
./lsb_steg -e beautiful.bmp secret.txt                 # output: steged_img.bmp
./lsb_steg -e beautiful.bmp secret.txt my_stego.bmp    # custom output name
```

### Decode
```bash
./lsb_steg -d steged_img.bmp                # output: decoded<original extension>, e.g. decoded.txt
./lsb_steg -d steged_img.bmp secret_msg.txt # custom name
./lsb_steg -d steged_img.bmp secret_msg     # extension is added automatically -> secret_msg.txt
```

### Verify
```bash
./lsb_steg -e beautiful.bmp secret.txt
./lsb_steg -d steged_img.bmp
cmp secret.txt decoded.txt && echo "Identical"
```

## How it works

BMP layout: `[54 byte header][RGB pixel data]`. The header is copied untouched.
Everything after it is written into the LSBs, **8 image bytes per hidden byte, MSB first**:

```
magic string "#*"        2 bytes
extension length         4 bytes (32 bits)
extension (".txt")       n bytes
secret file size         4 bytes (32 bits)
secret file data         size bytes
```
The rest of the image is copied unchanged.

Capacity check: `image bytes (width x height x 3)  >=  8 x (2 + 4 + ext_len + 4 + file_size)`.

## Error handling

- Missing / wrong arguments -> usage message
- Input or output not a `.bmp`; secret file with no extension (or longer than 8 chars)
- Cannot open a file
- Empty secret file
- Image too small for the secret file
- Image is not an uncompressed 24-bit BMP
- Source and output image are the same file
- Decoding an image with no hidden data (magic string missing) or with corrupted size fields

## Limits

- Only uncompressed 24-bit BMP files.
- Secret file must be smaller than 4 GB and fit in the image (about `width*height*3/8` bytes).
- No encryption: the data is hidden, not protected. Encrypt it first if it is sensitive.
