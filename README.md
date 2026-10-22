# LSB Image Steganography in C

> Hide a secret file inside an ordinary `.bmp` picture, and get it back later, with the picture looking exactly the same.

```
secret.txt  +  beautiful.bmp   ──►  ./lsb_steg -e  ──►  steged_img.bmp   (looks identical)
steged_img.bmp                 ──►  ./lsb_steg -d  ──►  decoded.txt      (same as secret.txt)
```

---

## Table of contents

1. [The process in short](#1-the-process-in-short)
2. [Theory explained simply](#2-theory-explained-simply)
3. [Setup and running the project](#3-setup-and-running-the-project)
4. [Files and what each one does](#4-files-and-what-each-one-does)
5. [List of functions](#5-list-of-functions)
6. [What each function does in detail](#6-what-each-function-does-in-detail)
7. [The encode cycle and the decode cycle](#7-the-encode-cycle-and-the-decode-cycle)
8. [How the functions are chained together](#8-how-the-functions-are-chained-together)
9. [Why the commands are built this way](#9-why-the-commands-are-built-this-way)
10. [How to test it and conclude it works](#10-how-to-test-it-and-conclude-it-works)
11. [Errors handled](#11-errors-handled) and [Limits](#12-limits)

---

## 1. The process in short

This project does **image steganography**: it hides data inside a picture so nobody suspects the data is there.

**Encoding (`-e`)**
1. Open a 24-bit `.bmp` image and the secret file.
2. Check that the secret file fits inside the image.
3. Copy the 54-byte BMP header unchanged.
4. Hide a few labels (magic string, extension, file size) and then the secret file's bytes, **one bit per image byte**, by overwriting the *last bit* of each image byte.
5. Copy the rest of the image unchanged and save it as a new `.bmp`.

**Decoding (`-d`)**
1. Open the stego image and skip the 54-byte header.
2. Read the *last bit* of image bytes in the same order, rebuild the labels, check the magic string.
3. Use the labels to learn the file extension and the size, then rebuild the secret file byte by byte.

The new image differs from the original by **at most 1** in any byte (out of 255), which the human eye cannot see.

---

## 2. Theory explained simply

### 2.1 Steganography vs encryption

| | Steganography (this project) | Encryption |
|---|---|---|
| Goal | Hide that a message **exists** | Make a message **unreadable** |
| Data after processing | Unchanged, just hidden | Scrambled with a key |
| Needs a key? | No | Yes |
| Name for this project | **LSB steganography, bit-level hiding** | not encryption |

Anyone who knows the method can read the data, so if it is sensitive, **encrypt it first** and then hide it. The "256" you may have heard of is just the number of values one byte can hold (0 to 255); it is not part of this method.

### 2.2 Bits, bytes, MSB and LSB

A **byte** is 8 **bits**, each 0 or 1. The leftmost bit is worth the most (**MSB**, most significant bit) and the rightmost bit is worth the least (**LSB**, least significant bit).

```
 bit position :  7    6    5    4    3    2    1    0
 weight       : 128   64   32   16    8    4    2    1
                MSB                                  LSB

 Value 200 = 1 1 0 0 1 0 0 0
 Flip the MSB  -> 0 1 0 0 1 0 0 0 = 72   (changed by 128, very visible)
 Flip the LSB  -> 1 1 0 0 1 0 0 1 = 201  (changed by 1, invisible)
```

That is the whole trick: **the LSB is the least important bit, so we can use it to store our own data.**

### 2.3 What is in a BMP file

A 24-bit BMP is simple and uncompressed:

```
┌──────────────────────┬──────────────────────────────────────────┐
│ 54 bytes header      │ pixel data: B G R  B G R  B G R  ...     │
│ (size, width, height)│ every pixel = 3 bytes (blue, green, red) │
└──────────────────────┴──────────────────────────────────────────┘
```

| Header offset | Meaning | Used by this project |
|---|---|---|
| 0 to 1 | `BM` signature | verified |
| 18 to 21 | width in pixels | capacity calculation |
| 22 to 25 | height in pixels | capacity calculation |
| 28 to 29 | bits per pixel | must be 24 |
| 30 to 33 | compression | must be 0 |

Because BMP is **uncompressed**, the bytes we change stay exactly as we wrote them. In a JPEG, compression would destroy our hidden bits, which is why this project uses BMP.

### 2.4 One character is not one pixel

A character is 8 bits and we hide **one bit per image byte**, so **one character needs 8 image bytes**. A pixel is 3 bytes, so one character spans about 2.67 pixels. The pixels are not picked at random: the code starts right after the header and uses the image bytes **in order**.

### 2.5 The code for this

**Encoding, in `encode_byte_to_lsb`:**

```c
image_buffer[i] = (image_buffer[i] & 0xFE) | ((data >> (7 - i)) & 1);
```

* `& 0xFE` clears the last bit of the image byte (`0xFE` is `11111110`).
* `(data >> (7-i)) & 1` picks bit `i` of the character, from the most significant bit first.
* `|` puts that bit into the cleared position.

**Decoding, in `decode_byte_from_lsb`:**

```c
data = (data << 1) | (image_buffer[i] & 1);
```

* `image_buffer[i] & 1` reads only the last bit of the image byte.
* `data << 1` makes room by shifting the bits collected so far one place to the left.
* `|` adds the new bit at the right end. After 8 image bytes, `data` is the original character.

### 2.6 Worked example: hiding the letter `M`

`M` is 77 in ASCII, which is `01001101` in binary. We take 8 image bytes and replace only their last bit:

| # | Image byte before | Binary before | Bit of `M` | Binary after | Byte after |
|---|---|---|---|---|---|
| 0 | 200 | 1100100**0** | **0** | 1100100**0** | 200 |
| 1 | 53  | 0011010**1** | **1** | 0011010**1** | 53 |
| 2 | 167 | 1010011**1** | **0** | 1010011**0** | 166 |
| 3 | 16  | 0001000**0** | **0** | 0001000**0** | 16 |
| 4 | 95  | 0101111**1** | **1** | 0101111**1** | 95 |
| 5 | 34  | 0010001**0** | **1** | 0010001**1** | 35 |
| 6 | 129 | 1000000**1** | **0** | 1000000**0** | 128 |
| 7 | 230 | 1110011**0** | **1** | 1110011**1** | 231 |

Some bytes did not change at all (the bit already matched) and none changed by more than 1.

**Decoding** just reads the last column of bits back: `0 1 0 0 1 1 0 1` = `01001101` = 77 = `M`.

### 2.7 Real example from this project

Encoding `secret.txt` into `beautiful.bmp` and looking at the 16 bytes right after the header (decimal values, taken from a real run):

```
original : 2 0 1 5 2 4 8 5   7 8 5 7 3 2 4 2
steged   : 2 0 1 4 2 4 9 5   6 8 5 6 3 2 5 2
LSBs     : 0 0 1 0 0 0 1 1   0 0 1 0 1 0 1 0
           └── '#' = 0x23 ──┘ └── '*' = 0x2A ──┘
```

The last bits of the stego bytes spell `00100011 00101010`, which is exactly `#*`, the magic string.

### 2.8 What exactly is hidden, and in what order

```
Image byte position      What is stored                    How many image bytes
────────────────────     ───────────────────────────────   ────────────────────
0   to 53                BMP header (copied, not changed)  54
54  to 69                magic string "#*"                 2 chars  x 8  = 16
70  to 101               extension length (a number)       32 bits       = 32
102 to 133               extension ".txt"                  4 chars  x 8  = 32
134 to 165               secret file size (a number)       32 bits       = 32
166 to 365               secret file data (25 chars)       25 chars x 8  = 200
366 to end               rest of image (copied)            unchanged
```

(Positions are for `secret.txt`, 25 bytes. A bigger secret file uses more image bytes.)

**Why this order?** The decoder knows nothing in advance. The magic string proves data exists. The extension length says how many characters to read next. The extension says what to name the output. The file size says exactly when to stop. Numbers are stored in 32 bits and characters in 8 bits, always MSB first.

### 2.9 Capacity

```
image capacity (bytes) = width x height x 3
bytes that must fit    = 2 (magic) + 4 (extn length) + len(extn) + 4 (size) + file size
requirement            : capacity  >=  8 x bytes that must fit
```

For `beautiful.bmp`: 1024 x 768 x 3 = **2,359,296** image bytes, so it can hold about **294,900 bytes** of secret data. BMP rows are padded to a multiple of 4 bytes, so the real data area can be a little larger than width x height x 3. The check uses the smaller number, so it never accepts a file that would not fit.

---

## 3. Setup and running the project

### 3.1 Requirements

Linux, macOS or WSL with `gcc` and `make`:

```bash
sudo apt update && sudo apt install -y build-essential     # Debian / Ubuntu
gcc --version && make --version                             # check
```

### 3.2 Get the code

```bash
git clone https://github.com/qbi777/C-SystemDesign.git
cd C-SystemDesign
git checkout lsb-steganography        # the branch with this project
cd lsb_steganography
```

### 3.3 Build

```bash
make
```

You should see four `gcc` lines and a new file `lsb_steg`. No warnings should appear.

### 3.4 Run it and observe the output

```bash
./lsb_steg                                  # prints usage (encode and decode)
./lsb_steg -e beautiful.bmp secret.txt      # hide secret.txt, creates steged_img.bmp
./lsb_steg -d steged_img.bmp                # recover it, creates decoded.txt
cat decoded.txt                             # prints: My password is SECRET ;)
```

Expected output of the encode step:

```
INFO: Output File not mentioned. Creating steged_img.bmp as default
INFO: Opening required files
INFO: Opened beautiful.bmp
INFO: Opened secret.txt
INFO: Opened steged_img.bmp
INFO: Done
INFO: ## Encoding Procedure Started ##
INFO: Checking for secret.txt size
INFO: Done. Not Empty
INFO: Checking for beautiful.bmp capacity to handle secret.txt
INFO: Done. Found OK
INFO: Copying Image Header
INFO: Done
INFO: Encoding Magic String Signature
INFO: Done
INFO: Encoding secret.txt File Extension
INFO: Done
INFO: Encoding secret.txt File Size
INFO: Done
INFO: Encoding secret.txt File Data
INFO: Done
INFO: Copying Left Over Data
INFO: Done
INFO: ## Encoding Done Successfully ##
```

Expected output of the decode step:

```
INFO: ## Decoding Procedure Started ##
INFO: Opening required files
INFO: Opened steged_img.bmp
INFO: Decoding Magic String Signature
INFO: Done
INFO: Decoding Output File Extension
INFO: Done
INFO: Output File not mentioned. Creating decoded.txt as default
INFO: Opened decoded.txt
INFO: Done. Opened all required files
INFO: Decoding decoded.txt File Size
INFO: Done
INFO: Decoding decoded.txt File Data
INFO: Done
INFO: ## Decoding Done Successfully ##
```

### 3.5 All command forms

```bash
# Encode:  ./lsb_steg -e <.bmp file> <secret file> [output .bmp]
./lsb_steg -e beautiful.bmp secret.txt                  # output  = steged_img.bmp
./lsb_steg -e beautiful.bmp secret.txt my_stego.bmp     # output  = my_stego.bmp

# Decode:  ./lsb_steg -d <.bmp file> [output file]
./lsb_steg -d my_stego.bmp                              # output  = decoded.txt
./lsb_steg -d my_stego.bmp secret_msg.txt               # output  = secret_msg.txt
./lsb_steg -d my_stego.bmp secret_msg                   # output  = secret_msg.txt (extension added)
```

### 3.6 Look at the images

```bash
eog beautiful.bmp steged_img.bmp &       # Linux image viewer; the two look identical
```

### 3.7 Clean up

```bash
make clean        # removes object files, the program, and generated images/outputs
```

### 3.8 Use your own image

The program only accepts **uncompressed 24-bit BMP**. Convert any picture with ImageMagick:

```bash
convert photo.jpg -type TrueColor BMP3:photo.bmp
```

---

## 4. Files and what each one does

| File | Purpose |
|---|---|
| `main.c` | Entry point. Reads the command line, decides encode or decode, prints usage, returns an exit code. |
| `encode.h` | `EncodeInfo` structure and prototypes of all encoding functions. |
| `encode.c` | Everything for encoding: argument checks, file opening, capacity check, bit hiding, copying. |
| `decode.h` | `DecodeInfo` structure and prototypes of all decoding functions. |
| `decode.c` | Everything for decoding: argument checks, bit reading, rebuilding the secret file. |
| `common.h` | Shared constants (magic string, header size, default names) and two small helpers for file extensions. |
| `types.h` | Custom types: `uint`, `Status` (`e_success`/`e_failure`), `OperationType`. |
| `test_encode.c` | Tiny driver to test `open_files()` and `get_image_size_for_bmp()` alone. |
| `run_tests.sh` | Automated test script (20 checks). |
| `Makefile` | Build rules: `make`, `make test`, `make test_encode`, `make clean`. |
| `README.md` | This document. |
| `.gitignore` | Keeps build output and generated files out of git. |
| `beautiful.bmp` | Sample 1024 x 768 24-bit carrier image. |
| `secret.txt` | Sample secret message. |

---

## 5. List of functions

**`main.c`**
`main`, `print_encode_usage`, `print_decode_usage`

**`common.h`**
`get_file_extension`, `has_bmp_extension`

**`encode.c`**
`check_operation_type`, `read_and_validate_encode_args`, `open_files`, `close_encode_files`, `get_file_size`, `get_image_size_for_bmp`, `check_bmp_format`, `check_capacity`, `copy_bmp_header`, `encode_byte_to_lsb`, `encode_size_to_lsb`, `encode_data_to_image`, `encode_uint`, `encode_magic_string`, `encode_secret_file_extn_size`, `encode_secret_file_extn`, `encode_secret_file_size`, `encode_secret_file_data`, `copy_remaining_img_data`, `do_encoding`

**`decode.c`**
`read_and_validate_decode_args`, `open_stego_file`, `open_output_file`, `close_decode_files`, `decode_byte_from_lsb`, `decode_size_from_lsb`, `decode_data_from_image`, `decode_uint`, `decode_magic_string`, `decode_secret_file_extn`, `decode_secret_file_size`, `decode_secret_file_data`, `do_decoding`

(`print_*_usage`, `check_bmp_format`, `encode_uint` and `decode_uint` are `static`: private helpers used only inside their own file.)

---

## 6. What each function does in detail

### 6.1 `main.c` and `common.h`

| Function | What it does |
|---|---|
| `main` | Checks `argc`, calls `check_operation_type`, then runs the encode path or the decode path. Closes files afterwards and returns `0` on success or `1` on failure. |
| `print_encode_usage` / `print_decode_usage` | Print the one-line usage text using `argv[0]` as the program name. |
| `get_file_extension` | Returns a pointer to the `.ext` part of a file name, or `NULL` if there is none. Ignores dots in directory names and hidden files like `.bashrc`. |
| `has_bmp_extension` | Returns 1 if the name ends with `.bmp` (any letter case). |

### 6.2 Shared by encode and decode flow: `encode.c` utilities

| Function | What it does |
|---|---|
| `check_operation_type` | Looks at `argv[1]`: `-e` gives `e_encode`, `-d` gives `e_decode`, anything else gives `e_unsupported`. |
| `get_file_size` | `fseek` to the end, `ftell` for the position (that is the size), then `rewind`. |
| `get_image_size_for_bmp` | Seeks to byte 18, reads width and height (4 bytes each) and returns width x height x 3. Handles negative height (top-down BMP). |
| `check_bmp_format` | Verifies the `BM` signature, 24 bits per pixel, and no compression. |

### 6.3 Encoding functions (`encode.c`)

| Function | What it does |
|---|---|
| `read_and_validate_encode_args` | Checks the image is `.bmp`, the secret file exists as an argument and has a usable extension (up to 8 chars), and the optional output is `.bmp`. Uses `steged_img.bmp` if none is given. Refuses when input and output are the same file. |
| `open_files` | Opens source image (`rb`), secret file (`rb`), stego image (`wb`). Prints `perror` and returns failure if any open fails. |
| `close_encode_files` | Closes all three files if they are open. |
| `check_capacity` | Gets the secret size (rejects empty files), validates the BMP format, computes capacity and compares with the bits needed (see 2.9). |
| `copy_bmp_header` | `rewind`s the source, reads 54 bytes and writes them to the output. Afterwards both file pointers sit at byte 54. |
| `encode_byte_to_lsb` | Loops 8 times and stores the 8 bits of one character, MSB first, into the last bits of 8 image bytes. |
| `encode_size_to_lsb` | Same idea for a 32-bit number over 32 image bytes. |
| `encode_data_to_image` | For each character: read 8 bytes from the source image, call `encode_byte_to_lsb`, write the 8 changed bytes to the output. |
| `encode_uint` | Reads 32 image bytes, calls `encode_size_to_lsb`, writes them out. Used for both size fields. |
| `encode_magic_string` | Hides `#*` using `encode_data_to_image`. |
| `encode_secret_file_extn_size` | Hides the length of the extension (for `.txt` that is 4). |
| `encode_secret_file_extn` | Hides the extension characters. |
| `encode_secret_file_size` | Hides the secret file size. |
| `encode_secret_file_data` | `rewind`s the secret file and hides it one byte at a time. Confirms every byte was read. |
| `copy_remaining_img_data` | Copies whatever is left of the source image to the output in 4096-byte blocks. |
| `do_encoding` | The controller. Calls everything above in the correct order and stops at the first failure. |

### 6.4 Decoding functions (`decode.c`)

| Function | What it does |
|---|---|
| `read_and_validate_decode_args` | Checks the stego image is `.bmp` and remembers the optional output name. |
| `open_stego_file` | Opens the stego image (`rb`). |
| `open_output_file` | Chooses the final name (`decoded<ext>` by default, `name<ext>` if the given name has no extension, otherwise the given name as is) and opens it (`wb`). Rejects names longer than 255 characters. |
| `close_decode_files` | Closes both files if they are open. |
| `decode_byte_from_lsb` | Reads the last bit of 8 image bytes and rebuilds one character. |
| `decode_size_from_lsb` | Reads the last bit of 32 image bytes and rebuilds a number. |
| `decode_data_from_image` | Reads 8 bytes per character from the file and calls `decode_byte_from_lsb` for each. |
| `decode_uint` | Reads 32 image bytes and returns the number they hide. |
| `decode_magic_string` | Skips the 54-byte header, decodes 2 characters and compares to `#*`. Fails with a clear message if they differ. |
| `decode_secret_file_extn` | Decodes the extension length, checks it is between 1 and 8, then decodes that many characters. |
| `decode_secret_file_size` | Decodes the file size and makes sure it is not larger than what remains in the image (protects against corrupted or fake images). |
| `decode_secret_file_data` | Decodes `size` characters, writing each into the output file. |
| `do_decoding` | The controller for decoding. Calls the functions above in order. |

---

## 7. The encode cycle and the decode cycle

### 7.1 Encode cycle

```
 1. START         ./lsb_steg -e beautiful.bmp secret.txt
 2. Identify      argv[1] is "-e"                                   -> encode mode
 3. Validate      .bmp image? secret has extension? output name?    -> else usage / error
 4. Open          source image, secret file, output image
 5. Check         secret not empty, BMP is 24-bit, image big enough -> else error, stop
 6. Header        copy 54 bytes unchanged
 7. Magic         hide "#*"                  (16 image bytes)
 8. Extn length   hide 4                     (32 image bytes)
 9. Extension     hide ".txt"                (32 image bytes)
10. File size     hide 25                    (32 image bytes)
11. File data     hide each byte of secret   (8 image bytes per byte)
12. Left over     copy the rest of the image unchanged
13. Close         close all files, return 0
```

### 7.2 Decode cycle

```
 1. START         ./lsb_steg -d steged_img.bmp
 2. Identify      argv[1] is "-d"                                   -> decode mode
 3. Validate      .bmp image? optional output name remembered
 4. Open          stego image
 5. Magic         skip 54 bytes, read 16 image bytes, must equal "#*" -> else "no hidden data"
 6. Extn length   read 32 image bytes -> 4 (must be 1..8)
 7. Extension     read 32 image bytes -> ".txt"
 8. Output file   choose a name using the extension, open it
 9. File size     read 32 image bytes -> 25 (must fit in the image)
10. File data     read 25 x 8 image bytes, write 25 characters to the output
11. Close         close all files, return 0
```

**Golden rule:** the decoder reads the fields in exactly the same order and with exactly the same bit widths as the encoder wrote them. The file position only moves forward, so each step starts where the previous one stopped.

---

## 8. How the functions are chained together

### 8.1 Encoding chain

```
main()
 ├─ check_operation_type()
 ├─ read_and_validate_encode_args()
 │    ├─ has_bmp_extension()
 │    └─ get_file_extension()
 ├─ do_encoding()
 │    ├─ open_files()
 │    ├─ check_capacity()
 │    │    ├─ get_file_size()
 │    │    ├─ check_bmp_format()
 │    │    └─ get_image_size_for_bmp()
 │    ├─ copy_bmp_header()
 │    ├─ encode_magic_string()            ──► encode_data_to_image() ──► encode_byte_to_lsb()
 │    ├─ encode_secret_file_extn_size()   ──► encode_uint()          ──► encode_size_to_lsb()
 │    ├─ encode_secret_file_extn()        ──► encode_data_to_image() ──► encode_byte_to_lsb()
 │    ├─ encode_secret_file_size()        ──► encode_uint()          ──► encode_size_to_lsb()
 │    ├─ encode_secret_file_data()        ──► encode_data_to_image() ──► encode_byte_to_lsb()
 │    └─ copy_remaining_img_data()
 └─ close_encode_files()
```

### 8.2 Decoding chain

```
main()
 ├─ check_operation_type()
 ├─ read_and_validate_decode_args()
 ├─ do_decoding()
 │    ├─ open_stego_file()
 │    ├─ decode_magic_string()        ──► decode_data_from_image() ──► decode_byte_from_lsb()
 │    ├─ decode_secret_file_extn()    ──► decode_uint()            ──► decode_size_from_lsb()
 │    │                               └─► decode_data_from_image() ──► decode_byte_from_lsb()
 │    ├─ open_output_file()
 │    ├─ decode_secret_file_size()    ──► decode_uint()            ──► decode_size_from_lsb()
 │    └─ decode_secret_file_data()    ──► decode_data_from_image() ──► decode_byte_from_lsb()
 └─ close_decode_files()
```

### 8.3 Three layers

| Layer | Encode | Decode | Job |
|---|---|---|---|
| Controller | `do_encoding` | `do_decoding` | Decides the order of steps |
| Field level | `encode_magic_string`, `encode_secret_file_size`, ... | `decode_magic_string`, `decode_secret_file_size`, ... | One piece of information each |
| Bit level | `encode_byte_to_lsb`, `encode_size_to_lsb` | `decode_byte_from_lsb`, `decode_size_from_lsb` | The actual bit manipulation |

Every function returns a `Status` (`e_success` or `e_failure`). A controller checks it after each step and stops at the first `e_failure`, so one problem never causes a chain of confusing errors.

---

## 9. Why the commands are built this way

### 9.1 `./lsb_steg -e beautiful.bmp secret.txt`

When you type a command, the shell splits it into words and hands them to `main` as `argc` (the count) and `argv` (the words):

| `argv[0]` | `argv[1]` | `argv[2]` | `argv[3]` | `argv[4]` | `argc` |
|---|---|---|---|---|---|
| `./lsb_steg` | `-e` | `beautiful.bmp` | `secret.txt` | *(optional)* `my_stego.bmp` | 4 or 5 |
| `./lsb_steg` | `-d` | `steged_img.bmp` | *(optional)* `out.txt` | | 3 or 4 |

* **`./`**: the program is in the current folder, and the shell does not search the current folder for programs unless you give the path.
* **`lsb_steg`**: the program name. `make` builds it from `main.c`, `encode.c` and `decode.c`.
* **`-e` / `-d`**: a *mode switch*. One program does two opposite jobs, so the first argument selects the job. Single-letter options with a dash are the standard Linux convention (`ls -l`, `rm -r`). `check_operation_type()` reads `argv[1]` and picks the path.
* **File names in a fixed order**: the program has no way to guess which file is which, so the order is fixed: image first, secret second, output last.
* **Optional output name**: in square brackets in the usage text. If it is missing, defaults are used (`steged_img.bmp`, `decoded.<ext>`), so the shortest command still works.
* **`argc` checks in `main`**: they give the right usage message when something is missing (`argc < 4` for encode, `argc < 3` for decode) or extra (`argc > 5`, `argc > 4`).
* **`.bmp` required**: the design only supports BMP, so a wrong extension is rejected before any file is touched.

### 9.2 Why the decoder is told only the image

The extension and size are stored inside the image, so the decoder needs nothing else. That is why `./lsb_steg -d steged_img.bmp` is enough and the default output name can have the right extension.

### 9.3 Build command details (`Makefile`)

```bash
gcc -Wall -Wextra -std=c99 -O2 -c main.c -o main.o      # compile one file
gcc -Wall -Wextra -std=c99 -O2 -o lsb_steg main.o encode.o decode.o   # link them
```

| Part | Why |
|---|---|
| `make` | Reads the `Makefile`, rebuilds only the files that changed. |
| `-c` | Compile only, produce an object file (`.o`). |
| `-o name` | Name of the output file. |
| `-Wall -Wextra` | Turn on warnings, which catch many bugs early. |
| `-std=c99` | Use the C99 standard, keeping the code portable. |
| `-O2` | Optimize; the byte-by-byte loops run faster. |
| `%.o: %.c encode.h decode.h types.h common.h` | If any header changes, every `.c` file is rebuilt. |

### 9.4 Test and inspection commands

| Command | Why it is used |
|---|---|
| `cmp a b` | Compares two files byte by byte. Silent and exit code 0 when identical. The most reliable way to prove the decoded file equals the original. |
| `cmp -n 54 a b` | Compares only the first 54 bytes (the header). |
| `cmp -i 366 a b` | Skips the first 366 bytes of both files, then compares the rest. |
| `cmp -l a b` | Lists every differing byte (position and the two values in octal). |
| `wc -l` | Counts lines, so `cmp -l ... | wc -l` counts differing bytes. |
| `od -An -tu1 -j54 -N16 file` | Prints 16 bytes from offset 54 as decimal numbers. |
| `echo $?` | Prints the exit code of the last command (`0` success, `1` failure). |
| `2>&1` | Sends error messages to the same place as normal output, so you can see both together. |

---

## 10. How to test it and conclude it works

### 10.1 One command

```bash
make test
```

This runs `run_tests.sh`, which performs 20 checks. A healthy run ends with:

```
Result: 20 passed, 0 failed
```

### 10.2 Manual test 1: the round trip (the main proof)

```bash
./lsb_steg -e beautiful.bmp secret.txt
./lsb_steg -d steged_img.bmp
cmp secret.txt decoded.txt && echo "IDENTICAL: decoding works"
```

`cmp` printing nothing and then `IDENTICAL` means every byte of the recovered file equals the original.

### 10.3 Manual test 2: the image really contains data

```bash
cmp beautiful.bmp steged_img.bmp && echo same || echo "different (good: data was hidden)"
cmp -l beautiful.bmp steged_img.bmp | wc -l
```

The first command reports *different*. For the 25-byte sample about **158** bytes differ (roughly half of the 312 bytes that were used, because a bit only changes the byte when it differs from the old last bit).

### 10.4 Manual test 3: nothing outside the hidden area changed

```bash
cmp -n 54  beautiful.bmp steged_img.bmp && echo "header untouched"
cmp -i 366 beautiful.bmp steged_img.bmp && echo "rest of image untouched"
ls -l beautiful.bmp steged_img.bmp      # same size, 2359350 bytes each
```

The number 366 is 54 (header) + 8 x (2 + 4 + 4 + 4 + 25) = 54 + 312.

### 10.5 Manual test 4: the change is invisible (at most 1)

```bash
cmp -l beautiful.bmp steged_img.bmp | awk '
function o(x,i,r){r=0;for(i=1;i<=length(x);i++)r=r*8+substr(x,i,1);return r}
{d=o($2)-o($3); if(d<0)d=-d; if(d>m)m=d} END{print "max difference:", m}'
```

Expected: `max difference: 1`. (`cmp -l` prints octal values, so the function converts them.)

### 10.6 Manual test 5: see the hidden magic string with your own eyes

```bash
# the 16 bytes right after the header, before and after encoding
od -An -tu1 -j54 -N16 beautiful.bmp
od -An -tu1 -j54 -N16 steged_img.bmp

# take only the last bit of each of those 16 bytes
od -An -tu1 -v -w1 -j54 -N16 steged_img.bmp | awk '{printf "%d", $1%2} END{print ""}'
```

The last command prints `0010001100101010`, which is `00100011` (`#`) followed by `00101010` (`*`). You have just decoded the magic string by hand.

To read the extension length the same way, use `-j70 -N32`; the result is `00000000000000000000000000000100` (= 4).

### 10.7 Manual test 6: look at the pictures

```bash
eog beautiful.bmp steged_img.bmp &
```

Flip between them. You will not see a difference.

### 10.8 Manual test 7: other kinds of files

```bash
./lsb_steg -e beautiful.bmp main.c code.bmp && ./lsb_steg -d code.bmp recovered
cmp main.c recovered.c && echo "source file recovered, extension .c kept"
```

### 10.9 Manual test 8: errors must be refused

| Command | Expected message |
|---|---|
| `./lsb_steg` | Both usage lines |
| `./lsb_steg -e beautiful` | Encoding usage line |
| `./lsb_steg -e notes.txt secret.txt` | `Source image must be a .bmp file` |
| `./lsb_steg -e beautiful.bmp missing.txt` | `Unable to open file missing.txt` |
| `head -c 3000000 /dev/zero > big.txt; ./lsb_steg -e beautiful.bmp big.txt` | `doesn't have the capacity to encode` |
| `./lsb_steg -d beautiful.bmp` | `Magic string not found` (a normal image has no hidden data) |
| `./lsb_steg -e beautiful.bmp secret.txt beautiful.bmp` | `Source and output image cannot be the same file` |

After each failing command, `echo $?` prints `1`.

### 10.10 How to conclude

The program is working correctly if **all** of these are true:

| # | Evidence | Command |
|---|---|---|
| 1 | Decoded file is byte-identical to the original | `cmp secret.txt decoded.txt` |
| 2 | Stego image differs from the original | `cmp beautiful.bmp steged_img.bmp` |
| 3 | Header and tail are untouched | `cmp -n 54 ...` and `cmp -i 366 ...` |
| 4 | No byte changed by more than 1 | the awk check in 10.5 |
| 5 | Hidden bits read by hand spell `#*` | the `od` check in 10.6 |
| 6 | Bad input is rejected with a clear message and exit code 1 | the table in 10.9 |
| 7 | Automated suite passes | `make test` gives `20 passed, 0 failed` |

---

## 11. Errors handled

* Missing or wrong arguments: usage message
* Source or output image that is not `.bmp`
* Secret file without an extension, or an extension longer than 8 characters
* A file that cannot be opened
* An empty secret file
* An image too small for the secret file
* An image that is not an uncompressed 24-bit BMP
* Source and output image being the same file
* Decoding an image with no hidden data (magic string missing)
* Decoding an image with corrupted extension or size fields
* Output file names that are too long

## 12. Limits

* Only uncompressed **24-bit BMP** files are supported.
* The secret file must be under 4 GB and must fit in the image (about width x height x 3 / 8 bytes).
* There is **no encryption**. The data is hidden, not protected. Anyone who knows the method can extract it.
* Saving the stego image as JPEG, resizing it, or re-compressing it destroys the hidden data. Keep it as BMP.
* The optional `-p` custom magic string shown in the design flowchart is not implemented.
