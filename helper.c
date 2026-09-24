#include "contact.h"
#include <ctype.h>    /* isdigit, isalpha, isalnum */
#include <stdlib.h>   /* general utilities (included for future use) */

/* =========================================================
   HELPER FUNCTIONS
   =========================================================
   Small utility functions shared across the entire project.
   Covers:
     - Safe input reading and buffer cleaning
     - Name, phone, and email validation with specific errors
     - Duplicate checking for phone and email
   ========================================================= */


/* ---------------------------------------------------------
   remove_newline
   Removes the trailing '\n' that fgets() leaves in the
   string. strcspn finds the index of '\n'; writing '\0'
   there terminates the string at that position.
   --------------------------------------------------------- */
void remove_newline(char string[])
{
    string[strcspn(string, "\n")] = '\0';
}


/* ---------------------------------------------------------
   read_string
   Reads a line of text safely using fgets().
   - Reads at most (size - 1) characters, adds '\0'.
   - Strips the trailing '\n' with remove_newline().
   - Prevents buffer overflows; handles spaces in input.
   --------------------------------------------------------- */
void read_string(char string[], int size)
{
    if (fgets(string, size, stdin) != NULL)
    {
        remove_newline(string);
    }
}


/* ---------------------------------------------------------
   clear_input_buffer
   Discards every character left in stdin after a scanf.
   Without this, the leftover '\n' would be picked up by
   the next fgets/read_string call, making it appear skipped.
   --------------------------------------------------------- */
void clear_input_buffer(void)
{
    int character;

    while ((character = getchar()) != '\n' &&
           character != EOF)
    {
        /* Discard characters until the buffer is clean */
    }
}


/* ---------------------------------------------------------
   validate_name
   ---------------------------------------------------------
   Checks the name against every rule from the spec and
   writes a specific reason into 'error_msg' if invalid.

   Rules enforced:
     1. Must be at least 4 characters long.
     2. Must contain only alphabets (A-Z, a-z) and spaces.
     3. No digits allowed.
     4. No symbols allowed.

   Returns 1 if valid, 0 if invalid.
   The caller prints error_msg when 0 is returned.

   isalpha() returns non-zero for A-Z and a-z.
   --------------------------------------------------------- */
int validate_name(char name[], char error_msg[])
{
    int index;
    int length;

    length = (int)strlen(name);

    /* Rule 1: Minimum 4 characters */
    if (length < 4)
    {
        strcpy(error_msg, "Name must contain at least 4 characters.");
        return 0;
    }

    /* Rules 2, 3, 4: Only alphabets and spaces allowed */
    for (index = 0; index < length; index++)
    {
        /* Allow spaces between first and last name */
        if (name[index] == ' ')
        {
            continue;
        }

        /* Digits are explicitly not allowed */
        if (isdigit((unsigned char)name[index]))
        {
            strcpy(error_msg, "Numbers are not allowed in the name.");
            return 0;
        }

        /* Anything that is not an alphabet is a symbol here */
        if (!isalpha((unsigned char)name[index]))
        {
            strcpy(error_msg, "Symbols are not allowed in the name.");
            return 0;
        }
    }

    /* All checks passed */
    error_msg[0] = '\0';
    return 1;
}


/* ---------------------------------------------------------
   validate_mobile
   ---------------------------------------------------------
   Checks a phone number against every rule from the spec
   and writes a specific reason into 'error_msg' if invalid.

   Rules enforced:
     1. Must be exactly 10 digits.
     2. First digit must be 6, 7, 8, or 9.
     3. Only digits allowed — no alphabets, no symbols.

   Returns 1 if valid, 0 if invalid.
   --------------------------------------------------------- */
int validate_mobile(char mobile[], char error_msg[])
{
    int index;
    int length;

    length = (int)strlen(mobile);

    /* Rules 3 & 4: Check every character is a digit first
       so we give the most specific error for non-digit input */
    for (index = 0; index < length; index++)
    {
        if (isalpha((unsigned char)mobile[index]))
        {
            strcpy(error_msg, "Alphabets are not allowed in the phone number.");
            return 0;
        }

        if (!isdigit((unsigned char)mobile[index]))
        {
            strcpy(error_msg, "Symbols are not allowed in the phone number.");
            return 0;
        }
    }

    /* Rule 1: Exactly 10 digits */
    if (length != 10)
    {
        strcpy(error_msg, "Phone number must contain exactly 10 digits.");
        return 0;
    }

    /* Rule 2: First digit between 6 and 9 */
    if (mobile[0] < '6' || mobile[0] > '9')
    {
        strcpy(error_msg, "First digit must be between 6 and 9.");
        return 0;
    }

    error_msg[0] = '\0';
    return 1;
}


/* ---------------------------------------------------------
   validate_email
   ---------------------------------------------------------
   Checks an email against every rule from the spec and
   writes a specific reason into 'error_msg' if invalid.

   Rules enforced:
     1. Must contain exactly one '@'.
     2. Must contain at least one '.'.
     3. Multiple '@' symbols not allowed.
     4. '.' must appear after '@', not before.
     5. At least one character must exist between '@' and '.'.
        e.g.  asdf@gmail.com  valid
              asdf@.com       invalid
     6. No extra characters after the final domain extension.
        e.g.  abcd@gmail.com      valid
              abcd@gmail.com123   invalid
     7. Only '@' and '.' are allowed as symbols.
     8. After '@' and after '.' only alphabets or digits.

   Returns 1 if valid, 0 if invalid.
   --------------------------------------------------------- */
int validate_email(char email[], char error_msg[])
{
    int   index;
    int   length;
    int   at_count;
    char *at_ptr;
    char *dot_ptr;
    char *domain_ptr;   /* Points to the character just after '@' */
    char *ext_ptr;      /* Points to the character just after final '.' */

    length   = (int)strlen(email);
    at_count = 0;

    /* Count '@' symbols and check for forbidden symbols */
    for (index = 0; index < length; index++)
    {
        char c = email[index];

        if (c == '@')
        {
            at_count++;
            continue;
        }

        if (c == '.')
        {
            continue;
        }

        /* Digits and alphabets are always allowed */
        if (isalnum((unsigned char)c))
        {
            continue;
        }

        /* Anything else is a forbidden symbol */
        strcpy(error_msg, "Only '@' and '.' are allowed as symbols in the email.");
        return 0;
    }

    /* Rule 3 & 1: Must have exactly one '@' */
    if (at_count == 0)
    {
        strcpy(error_msg, "Email must contain an '@' symbol.");
        return 0;
    }

    if (at_count > 1)
    {
        strcpy(error_msg, "Multiple '@' symbols are not allowed.");
        return 0;
    }

    /* Locate '@' and final '.' */
    at_ptr  = strchr(email, '@');
    dot_ptr = strrchr(email, '.');

    /* Rule 4: '@' must not be the first character */
    if (at_ptr == email)
    {
        strcpy(error_msg, "There must be characters before '@'.");
        return 0;
    }

    /* Rule 2: Must have at least one '.' */
    if (dot_ptr == NULL)
    {
        strcpy(error_msg, "Email must contain a '.' (dot).");
        return 0;
    }

    /* Rule 4: '.' must appear after '@' */
    if (dot_ptr < at_ptr)
    {
        strcpy(error_msg, "The '.' must appear after '@', not before.");
        return 0;
    }

    /* Rule 5: At least one character between '@' and '.' */
    domain_ptr = at_ptr + 1;   /* First char after '@' */

    if (domain_ptr == dot_ptr)
    {
        strcpy(error_msg, "There must be at least one character between '@' and '.'.");
        return 0;
    }

    /* Rule 6: No extra characters after the final '.' extension */
    ext_ptr = dot_ptr + 1;     /* First char after final '.' */

    if (*ext_ptr == '\0')
    {
        strcpy(error_msg, "Domain extension is missing after the final '.'.");
        return 0;
    }

    /*
       Rule 6 & 8: After the final '.' only lowercase/uppercase
       alphabets are allowed — no digits, no symbols.
       e.g. ".com" ".net" ".org" are valid
            ".com123" is invalid (extra chars after extension)
    */
    while (*ext_ptr != '\0')
    {
        if (!isalpha((unsigned char)*ext_ptr))
        {
            strcpy(error_msg, "Extra characters found after the domain extension.");
            return 0;
        }
        ext_ptr++;
    }

    error_msg[0] = '\0';
    return 1;
}


/* ---------------------------------------------------------
   is_valid_mobile
   Thin wrapper kept for backward compatibility.
   Returns 1 if valid, 0 if not. Discards error message.
   --------------------------------------------------------- */
int is_valid_mobile(char mobile[])
{
    char error_msg[100];
    return validate_mobile(mobile, error_msg);
}


/* ---------------------------------------------------------
   is_valid_email
   Thin wrapper kept for backward compatibility.
   Returns 1 if valid, 0 if not. Discards error message.
   --------------------------------------------------------- */
int is_valid_email(char email[])
{
    char error_msg[100];
    return validate_email(email, error_msg);
}


/* ---------------------------------------------------------
   phone_already_exists
   Scans all contacts for an exact match of 'phone'.
   ignored_index is the contact currently being edited
   (pass -1 when adding a new contact).
   Returns 1 if a duplicate is found, 0 if unique.
   --------------------------------------------------------- */
int phone_already_exists(
    struct AddressBook *addressBook,
    char phone[],
    int ignored_index)
{
    int index;

    for (index = 0; index < addressBook->contactCount; index++)
    {
        if (index == ignored_index)
        {
            continue;  /* Skip the contact being edited */
        }

        if (strcmp(addressBook->contacts[index].phone, phone) == 0)
        {
            return 1;  /* Duplicate found */
        }
    }

    return 0;
}


/* ---------------------------------------------------------
   email_already_exists
   Same logic as phone_already_exists but checks email.
   Returns 1 if a duplicate is found, 0 if unique.
   --------------------------------------------------------- */
int email_already_exists(
    struct AddressBook *addressBook,
    char email[],
    int ignored_index)
{
    int index;

    for (index = 0; index < addressBook->contactCount; index++)
    {
        if (index == ignored_index)
        {
            continue;  /* Skip the contact being edited */
        }

        if (strcmp(addressBook->contacts[index].email, email) == 0)
        {
            return 1;  /* Duplicate found */
        }
    }

    return 0;
}
