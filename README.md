# 📒 Address Book — C Console Application

A terminal-based contact management system written in **pure C**.  
Supports adding, searching, editing, deleting, and listing contacts,  
with all data persisted to a CSV file between sessions.  
Every field is validated with **specific error messages** on every invalid input.

---

## Table of Contents

- [Features](#features)
- [Project Structure](#project-structure)
- [File Descriptions](#file-descriptions)
- [Data Storage](#data-storage)
- [Getting Started](#getting-started)
- [How to Use](#how-to-use)
- [Validation Rules](#validation-rules)
- [Error Messages Reference](#error-messages-reference)
- [Architecture Overview](#architecture-overview)
- [Compilation Flags Explained](#compilation-flags-explained)
- [Known Constraints](#known-constraints)

---

## Features

| Feature | Description |
|---|---|
| Add Contact | Collects name, phone, and email with full field-level validation |
| Search Contact | Search by name, phone, or email — shows **all** matches, not just the first |
| Edit Contact | Search → pick from matches → update any field → re-validates before saving |
| Delete Contact | Search → pick from matches → confirm with y/n → removes and saves |
| List Contacts | Displays all contacts in a clean numbered table |
| Persistent Storage | Auto-saves to `database.csv` after every add, edit, and delete |
| Duplicate Detection | Rejects duplicate phone numbers and email addresses across all contacts |
| Specific Error Messages | Every validation failure prints the exact reason, not a generic error |
| Exit Option in Every Menu | Search, Edit, and Delete menus all have an Exit option to return safely |
| Menu Choice Validation | Invalid menu input (letters, symbols, out-of-range numbers) is caught and re-asked |

---

## Project Structure

```
addressBook-Project01/
│
├── main.c             → Entry point. Initialises, loads data, runs the main menu loop
├── addressbook.c      → All contact operations (add, search, edit, delete, list)
├── helper.c           → Validators, input utilities, and duplicate checkers
├── contact.h          → Shared structs, constants, and all function prototypes
├── database.csv       → Auto-generated flat-file database (CSV format)
└── KEYWORDS_GUIDE.txt → Learning reference for every keyword used in the project
```

---

## File Descriptions

### `main.c`
The program entry point. Responsibilities:
- Initialises the `AddressBook` struct to zero
- Calls `pull_file_data()` to load saved contacts from `database.csv` on startup
- Runs the main menu in a `while(1)` loop
- Validates menu input — rejects non-numbers and out-of-range values with a clear error
- Dispatches to the correct operation via `switch`
- Calls `push_data_to_file()` and exits cleanly when the user chooses option 6

---

### `addressbook.c`
Contains all contact operations. Divided into public functions and private (`static`) helpers.

**Public functions:**

| Function | Purpose |
|---|---|
| `pull_file_data()` | Opens `database.csv` and reads all contacts using `fscanf` |
| `push_data_to_file()` | Overwrites `database.csv` with current in-memory contacts using `fprintf` |
| `add_contact()` | Collects name, phone, email — validates each — stores and saves |
| `search_contact()` | Menu-driven search with Exit option — shows all matching contacts |
| `edit_contact()` | Search → pick one → edit fields with re-validation → save |
| `delete_contact()` | Search → pick one → confirm → shift array → save |
| `list_contacts()` | Prints all contacts in a formatted numbered table |
| `search_by_name()` | Partial name match using `strstr()`, fills `index_record[]` |
| `search_by_phone()` | Partial phone match using `strstr()`, fills `index_record[]` |
| `search_by_gmail()` | Partial email match using `strstr()`, fills `index_record[]` |
| `mobile_verify()` | Prints a human-readable validation result for a phone number |
| `gmail_verify()` | Prints a human-readable validation result for an email address |

**Private (`static`) helpers — internal to `addressbook.c` only:**

| Static Function | Purpose |
|---|---|
| `input_name()` | Loops until a valid name is entered; prints specific error on each failure |
| `input_phone()` | Loops until a valid, non-duplicate phone is entered |
| `input_email()` | Loops until a valid, non-duplicate email is entered |
| `display_search_results()` | Prints all contacts in `index_record[]` as a numbered list |
| `pick_contact_from_results()` | Asks user to pick one contact by number; loops on bad input |
| `run_search_menu()` | Shared Name/Phone/Email/Exit search sub-menu for edit and delete |

---

### `helper.c`
All reusable utilities and validators. Every validator returns `1` (valid) or `0` (invalid)
and writes the specific failure reason into an `error_msg` buffer.

**Input utilities:**

| Function | Purpose |
|---|---|
| `read_string(str, size)` | Safe keyboard input using `fgets` — no buffer overflow, handles spaces |
| `remove_newline(str)` | Strips trailing `\n` that `fgets` leaves at the end of the string |
| `clear_input_buffer()` | Discards leftover characters in `stdin` after a `scanf` call |

**Validators (return 1 = valid, 0 = invalid; fill `error_msg` on failure):**

| Function | What it checks |
|---|---|
| `validate_name(name, error_msg)` | Length ≥ 4, alphabets and spaces only, no digits, no symbols |
| `validate_mobile(mobile, error_msg)` | Exactly 10 digits, first digit 6–9, no alphabets, no symbols |
| `validate_email(email, error_msg)` | All 8 email rules from the spec (see Validation Rules section) |

**Duplicate checkers:**

| Function | Purpose |
|---|---|
| `phone_already_exists(ab, phone, ignored_index)` | Returns 1 if phone is already used by another contact |
| `email_already_exists(ab, email, ignored_index)` | Returns 1 if email is already used by another contact |

The `ignored_index` parameter lets edit operations skip the contact being edited
so it does not flag its own existing values as duplicates. Pass `-1` when adding.

---

### `contact.h`
The shared header included by all three `.c` files. Contains:
- `#ifndef / #define / #endif` guards to prevent double inclusion
- `#define` constants: `MAX_CONTACTS`, `NAME_SIZE`, `PHONE_SIZE`, `EMAIL_SIZE`, `ERROR_SIZE`, `FILE_NAME`
- `struct Contact` — holds one person's name, phone, and email
- `struct AddressBook` — holds the full contact array, count, and search result tracking (`index_record[]`, `ir_size`)
- All function prototypes so every `.c` file knows about every function

### `database.csv`
Auto-generated flat-file database. Format:
```
Name,Phone,Email
Hubert Spencer,9876543210,hubert@gmail.com
Samyazaa,9360818091,samyazaa@mail.com
Lemin,9360818980,lemin@gmail.com
```
- Created automatically on the first save
- Overwritten completely each time contacts are saved
- Loaded in full each time the program starts
- The header row `Name,Phone,Email` is skipped during loading

---

## Data Storage

```
struct Contact                      database.csv row
───────────────────────────         ──────────────────────────────────
char name[50]           ←────────→  Hubert Spencer
char phone[15]          ←────────→  9876543210
char email[100]         ←────────→  hubert@gmail.com
```

The `AddressBook` struct holds up to **100 contacts** in memory at once.  
The `index_record[]` array stores the **positions** (indices) of contacts that matched
the most recent search — this is what lets Edit and Delete work across multiple
matches without hardcoding a fixed search strategy.

```
After searching "Spencer":
  index_record[0] = 0    →  contacts[0]  =  Hubert Spencer
  index_record[1] = 4    →  contacts[4]  =  Spencer
  ir_size = 2
```

---

## Getting Started

### Requirements
- GCC or any C99-compatible compiler
- A terminal (macOS Terminal, Linux shell, or Windows with MinGW / WSL)

### Compile

```bash
gcc -Wall -Wextra -pedantic main.c addressbook.c helper.c -o addressbook
```

> ⚠️ **All three `.c` files must be listed.** Omitting `helper.c` causes linker errors
> because all validators and input utilities live there. The `.h` file is **never** listed
> — it is pulled in automatically via `#include "contact.h"` inside each `.c` file.

### Run

```bash
./addressbook
```

---

## How to Use

### Main Menu
```
============================================
                MAIN MENU
============================================
  1. Add Contact
  2. Search Contact
  3. Edit Contact
  4. Delete Contact
  5. List Contacts
  6. Exit
============================================
  Enter your choice (1-6):
```

Any input outside `1–6`, or non-numeric input, prints an error and re-asks.

---

### Adding a Contact

The program asks for name, phone, and email one at a time.
Each field loops until valid input is received.

```
  Enter name  : Ab
  Error: Name must contain at least 4 characters.

  Enter name  : Alice123
  Error: Numbers are not allowed in the name.

  Enter name  : Alice
  Enter phone : 123
  Error: Phone number must contain exactly 10 digits.

  Enter phone : 9876543210
  Enter email : alice@.com
  Error: There must be at least one character between '@' and '.'.

  Enter email : alice@gmail.com
  Contact added successfully!
```

---

### Searching a Contact

```
====================================
          SEARCH CONTACT
====================================
  1. Search by Name
  2. Search by Phone
  3. Search by Email
  4. Exit Search
====================================
```

- Partial input works — searching `"ali"` matches `"Alice"` and `"Somali"`
- **All** matching contacts are displayed, not just the first one
- Option `4` returns to the main menu at any time

---

### Editing a Contact

1. A search sub-menu appears — find the contact by name, phone, or email
2. All matches are listed with numbers `[1], [2], ...`
3. Pick the contact by number (`0` to cancel)
4. Current values are shown — press **Enter** to keep any field unchanged
5. New values are re-validated with the same rules as Add Contact
6. Changes are saved to `database.csv` immediately

```
  New name  [Alice]: 
  New phone [9876543210]: 9999999999
  New email [alice@gmail.com]: alice@newmail.com

  Contact updated successfully!
```

---

### Deleting a Contact

1. A search sub-menu appears — find the contact by name, phone, or email
2. All matches are listed with numbers `[1], [2], ...`
3. Pick the contact by number (`0` to cancel)
4. The selected contact is shown in full and confirmation is requested
5. Enter `y` to confirm or `n` to cancel — any other input re-asks
6. On confirmation, the contact is removed and the file is updated

```
  Contact selected for deletion:
  Name  : Alice
  Phone : 9876543210
  Email : alice@gmail.com

  Are you sure you want to delete this contact? (y/n): y

  Contact deleted successfully!
```

---

## Validation Rules

### Name Validation

| Rule | Detail |
|---|---|
| Minimum length | Must be at least **4 characters** |
| Allowed characters | Alphabets (A–Z, a–z) and spaces only |
| Digits | Not allowed |
| Symbols | Not allowed |

### Phone Number Validation

| Rule | Detail |
|---|---|
| Length | Must be exactly **10 digits** |
| First digit | Must be **6, 7, 8, or 9** (Indian mobile standard) |
| Alphabets | Not allowed |
| Symbols | Not allowed |

### Email Validation

| Rule | Example |
|---|---|
| Must contain exactly one `@` | `a@@gmail.com` → invalid |
| Must contain at least one `.` | `alice@gmailcom` → invalid |
| `@` must not be the first character | `@gmail.com` → invalid |
| `.` must appear after `@`, not before | `alice.gmail@com` → invalid |
| At least one character between `@` and `.` | `alice@.com` → invalid |
| No extra characters after the final extension | `alice@gmail.com123` → invalid |
| Only `@` and `.` allowed as symbols | `alice#gmail.com` → invalid |
| Domain and extension: alphabets only after `.` | `alice@gmail.com123` → invalid |

### Duplicate Check

| Rule | Detail |
|---|---|
| Phone | No two contacts can share the same phone number |
| Email | No two contacts can share the same email address |
| Edit exception | The contact being edited is excluded from its own duplicate check |

---

## Error Messages Reference

### Name Errors

| Input | Error Message |
|---|---|
| `"Ali"` (3 chars) | `Name must contain at least 4 characters.` |
| `"Ali2"` | `Numbers are not allowed in the name.` |
| `"Al@n"` | `Symbols are not allowed in the name.` |

### Phone Errors

| Input | Error Message |
|---|---|
| `"abc1234567"` | `Alphabets are not allowed in the phone number.` |
| `"987654321@"` | `Symbols are not allowed in the phone number.` |
| `"98765"` | `Phone number must contain exactly 10 digits.` |
| `"1234567890"` | `First digit must be between 6 and 9.` |

### Email Errors

| Input | Error Message |
|---|---|
| `"alice#mail.com"` | `Only '@' and '.' are allowed as symbols in the email.` |
| `"alicegmail.com"` | `Email must contain an '@' symbol.` |
| `"alice@@gmail.com"` | `Multiple '@' symbols are not allowed.` |
| `"@gmail.com"` | `There must be characters before '@'.` |
| `"alice@.com"` | `There must be at least one character between '@' and '.'.` |
| `"alice@gmail."` | `Domain extension is missing after the final '.'.` |
| `"alice@gmail.com123"` | `Extra characters found after the domain extension.` |

---

## Architecture Overview

```
main.c
  │
  ├── pull_file_data()               Load CSV → memory on startup
  │
  ├── Main menu loop (with choice validation)
  │     │
  │     ├── add_contact()
  │     │     ├── input_name()           [static] → validate_name()
  │     │     ├── input_phone()          [static] → validate_mobile() + phone_already_exists()
  │     │     ├── input_email()          [static] → validate_email()  + email_already_exists()
  │     │     └── push_data_to_file()
  │     │
  │     ├── search_contact()  (with Exit option)
  │     │     └── search_by_name / search_by_phone / search_by_gmail()
  │     │           └── display_search_results()   [static]
  │     │
  │     ├── edit_contact()
  │     │     ├── run_search_menu()      [static]  (with Exit option)
  │     │     ├── pick_contact_from_results() [static]  (loops on bad input)
  │     │     ├── validate_name()        direct call with error_msg
  │     │     ├── validate_mobile()      direct call with error_msg
  │     │     ├── validate_email()       direct call with error_msg
  │     │     └── push_data_to_file()
  │     │
  │     ├── delete_contact()
  │     │     ├── run_search_menu()      [static]  (with Exit option)
  │     │     ├── pick_contact_from_results() [static]  (loops on bad input)
  │     │     ├── confirmation loop      (y/n only, re-asks on other input)
  │     │     └── push_data_to_file()
  │     │
  │     └── list_contacts()
  │
  └── push_data_to_file()             Save memory → CSV on exit
```

---

## Compilation Flags Explained

| Flag | What it does |
|---|---|
| `-Wall` | Enables all standard compiler warnings (unused variables, missing returns, etc.) |
| `-Wextra` | Enables additional warnings beyond `-Wall` (signed/unsigned comparisons, etc.) |
| `-pedantic` | Enforces strict ISO C compliance — rejects non-standard compiler extensions |
| `-o addressbook` | Names the output executable `addressbook` instead of the default `a.out` |

Running all three flags together is good practice — the compiler catches many
bugs at compile time before they become silent runtime crashes.

---

## Known Constraints

| Constraint | Detail |
|---|---|
| Max contacts | 100 — change `#define MAX_CONTACTS` in `contact.h` and recompile |
| Name max length | 49 characters (50 bytes including `'\0'`) |
| Phone max length | 14 characters (15 bytes including `'\0'`) |
| Email max length | 99 characters (100 bytes including `'\0'`) |
| Phone format | Indian mobile numbers only — must start with 6, 7, 8, or 9 |
| Name format | Alphabets and spaces only — no digits or special characters |
| Storage | Plain CSV — no encryption, no binary encoding |
| Concurrency | Single-user only — no file locking for simultaneous access |

---

*Built as a learning project in C — covering structs, file I/O, arrays, pointers,*  
*input validation, modular code organisation, and defensive programming.*
