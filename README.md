# 📒 Address Book — C Console Application

A terminal-based contact management system written in **pure C**.  
Supports adding, searching, editing, deleting, and listing contacts,  
with all data persisted to a CSV file between sessions.

---

## Table of Contents

- [Features](#features)
- [Project Structure](#project-structure)
- [File Descriptions](#file-descriptions)
- [Data Storage](#data-storage)
- [Getting Started](#getting-started)
- [How to Use](#how-to-use)
- [Validation Rules](#validation-rules)
- [Architecture Overview](#architecture-overview)
- [Compilation Flags Explained](#compilation-flags-explained)
- [Known Constraints](#known-constraints)

---

## Features

| Feature | Description |
|---|---|
| Add Contact | Stores name, phone, and email with full validation |
| Search Contact | Search by name, phone, or email — shows **all** matches |
| Edit Contact | Search first, pick from matches, update any field |
| Delete Contact | Search first, pick from matches, confirm then delete |
| List Contacts | Displays all contacts in a clean numbered table |
| Persistent Storage | Auto-saves to `database.csv` after every change |
| Duplicate Detection | Rejects duplicate phone numbers and email addresses |
| Input Validation | Validates Indian mobile numbers and email format |

---

## Project Structure

```
addressBook-Project01/
│
├── main.c            → Entry point. Loads data, runs the main menu loop
├── addressbook.c     → All contact operations (add, search, edit, delete, list)
├── helper.c          → Reusable utility functions (input, validation, duplicates)
├── contact.h         → Shared structs, constants, and all function prototypes
├── database.csv      → Auto-generated data file (created on first save)
└── KEYWORDS_GUIDE.txt → Learning reference for every keyword used in the project
```

---

## File Descriptions

### `main.c`
The program entry point. Responsibilities:
- Initialises the `AddressBook` struct
- Calls `pull_file_data()` to load saved contacts from `database.csv`
- Displays the main menu in a loop
- Dispatches to the correct function based on the user's choice
- Calls `push_data_to_file()` on exit to save all changes

### `addressbook.c`
Contains all contact operations. Key functions:

| Function | Purpose |
|---|---|
| `pull_file_data()` | Reads contacts from `database.csv` using `fscanf` |
| `push_data_to_file()` | Writes all contacts back to `database.csv` using `fprintf` |
| `add_contact()` | Collects and validates a new contact, then saves |
| `search_contact()` | Menu-driven search — shows all matching contacts |
| `edit_contact()` | Search → pick one match → edit fields → save |
| `delete_contact()` | Search → pick one match → confirm → shift array → save |
| `list_contacts()` | Prints all contacts in a formatted table |
| `search_by_name()` | Partial name match using `strstr()`, fills `index_record[]` |
| `search_by_phone()` | Partial phone match using `strstr()`, fills `index_record[]` |
| `search_by_gmail()` | Partial email match using `strstr()`, fills `index_record[]` |

Three internal (`static`) helpers are used only within this file:

| Static Function | Purpose |
|---|---|
| `display_search_results()` | Prints numbered list of all search matches |
| `pick_contact_from_results()` | Asks user to pick one match by number |
| `run_search_menu()` | Shared search sub-menu used by edit and delete |

### `helper.c`
Small, reusable utility functions used across the project:

| Function | Purpose |
|---|---|
| `read_string()` | Safe string input using `fgets` — prevents buffer overflow |
| `remove_newline()` | Strips the trailing `\n` left by `fgets` |
| `clear_input_buffer()` | Discards leftover characters after `scanf` |
| `is_valid_mobile()` | Validates a 10-digit Indian mobile number |
| `is_valid_email()` | Validates basic email format (`@` and `.` rules) |
| `phone_already_exists()` | Checks for duplicate phone across all contacts |
| `email_already_exists()` | Checks for duplicate email across all contacts |

### `contact.h`
The shared header file included by all three `.c` files. Contains:
- `#include` guards (`#ifndef / #define / #endif`) to prevent double inclusion
- `#define` constants: `MAX_CONTACTS`, `NAME_SIZE`, `PHONE_SIZE`, `EMAIL_SIZE`, `FILE_NAME`
- `struct Contact` — holds one person's name, phone, and email
- `struct AddressBook` — holds the contact array, count, and search result tracking
- All function prototypes so every file knows about every function

### `database.csv`
Auto-generated flat-file database. Format:
```
Name,Phone,Email
Hubert Spencer,9876543210,hubert@gmail.com
Samyazaa,9360818091,samyazaa@mail.com
Lemin,9360818980,lemin@gmail.com
```
- Created automatically on first save
- Overwritten completely each time contacts are saved
- Loaded in full each time the program starts

---

## Data Storage

```
struct Contact                     database.csv row
──────────────────────────         ─────────────────────────────────────
char name[50]          ←────────→  Hubert Spencer
char phone[15]         ←────────→  9876543210
char email[50]         ←────────→  hubert@gmail.com
```

The `AddressBook` struct holds up to **100 contacts** in memory.  
The `index_record[]` array stores the positions of contacts that matched  
the most recent search — this is what allows edit and delete to work  
on multiple matches without hardcoding a fixed search strategy.

---

## Getting Started

### Requirements
- GCC or any C99-compatible compiler
- A terminal (macOS Terminal, Linux shell, or Windows with MinGW/WSL)

### Compile

```bash
gcc -Wall -Wextra -pedantic main.c addressbook.c helper.c -o addressbook
```

> ⚠️ All three `.c` files must be listed. The `.h` file is never listed —  
> it is pulled in automatically by `#include "contact.h"` inside each `.c` file.

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
```

### Adding a Contact
- Enter a non-empty name
- Enter a valid 10-digit Indian mobile number (starts with 6, 7, 8, or 9)
- Enter a valid email address
- Duplicate phone or email will be rejected automatically

### Searching
- Choose to search by Name, Phone, or Email
- Partial input works — searching `"hu"` will match `"Hubert"` and `"Rahul"`
- **All** matching contacts are shown, not just the first one

### Editing a Contact
1. Search for the contact (by name, phone, or email)
2. All matches are listed with numbers `[1], [2], ...`
3. Pick the contact you want to edit
4. Press **Enter** to keep any existing field unchanged
5. Changes are saved to `database.csv` immediately

### Deleting a Contact
1. Search for the contact (by name, phone, or email)
2. All matches are listed with numbers `[1], [2], ...`
3. Pick the contact you want to delete
4. Confirm with `y` or cancel with `n`
5. The contact is removed and the file is updated immediately

---

## Validation Rules

### Phone Number
- Must be exactly **10 digits**
- First digit must be **6, 7, 8, or 9** (Indian mobile standard)
- Every character must be a digit — no spaces or dashes

### Email Address
- Must contain exactly one `@` symbol
- `@` must not be the first character
- Must contain a `.` after the `@`
- `.` must not be the last character

### Duplicate Check
- No two contacts can share the same phone number
- No two contacts can share the same email address
- During editing, the contact's own current values are excluded from the duplicate check

---

## Architecture Overview

```
main.c
  │
  ├── pull_file_data()          Load CSV → memory on startup
  │
  ├── Main menu loop
  │     ├── add_contact()
  │     │     ├── read_string()         helper.c
  │     │     ├── is_valid_mobile()     helper.c
  │     │     ├── is_valid_email()      helper.c
  │     │     ├── phone_already_exists() helper.c
  │     │     ├── email_already_exists() helper.c
  │     │     └── push_data_to_file()
  │     │
  │     ├── search_contact()
  │     │     └── search_by_name/phone/gmail()
  │     │           └── display_search_results()  [static]
  │     │
  │     ├── edit_contact()
  │     │     ├── run_search_menu()     [static]
  │     │     ├── pick_contact_from_results() [static]
  │     │     └── push_data_to_file()
  │     │
  │     ├── delete_contact()
  │     │     ├── run_search_menu()     [static]
  │     │     ├── pick_contact_from_results() [static]
  │     │     └── push_data_to_file()
  │     │
  │     └── list_contacts()
  │
  └── push_data_to_file()       Save memory → CSV on exit
```

---

## Compilation Flags Explained

| Flag | Meaning |
|---|---|
| `-Wall` | Enable all common compiler warnings |
| `-Wextra` | Enable additional warnings beyond `-Wall` |
| `-pedantic` | Enforce strict C standard compliance, reject non-standard extensions |
| `-o addressbook` | Name the output executable `addressbook` |

Running with all three warning flags is good practice — it catches bugs  
before they become runtime crashes.

---

## Known Constraints

| Constraint | Detail |
|---|---|
| Max contacts | 100 (defined by `MAX_CONTACTS` in `contact.h`) |
| Name length | Up to 49 characters |
| Phone length | Up to 14 characters |
| Email length | Up to 49 characters |
| Phone validation | Indian mobile numbers only (starts with 6/7/8/9) |
| Storage format | Plain CSV — no encryption or binary encoding |
| Concurrency | Single-user only — no file locking for concurrent access |

To increase the contact limit, change `#define MAX_CONTACTS 100` in `contact.h`  
and recompile. No other code needs to change.

---

*Built as a learning project in C — covering structs, file I/O, arrays, pointers, input validation, and modular code organisation.*
