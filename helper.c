#include "contact.h"
#include <ctype.h>  /* isdigit */
#include <stdlib.h> /* general utilities (included for future use) */

/* =========================================================
   HELPER FUNCTIONS
   =========================================================
   These are small utility functions used across the project.
   They handle:
     - Input reading and cleaning
     - Validation of phone and email
     - Duplicate checking
   ========================================================= */

/* ---------------------------------------------------------
   remove_newline
   ---------------------------------------------------------
   fgets() stores the newline character '\n' at the end of
   the string it reads. This function removes that '\n' by
   replacing it with a null terminator '\0'.

   strcspn(string, "\n") returns the number of characters
   before the first '\n'. Writing '\0' there cuts the string.
   --------------------------------------------------------- */
void remove_newline(char string[])
{
    string[strcspn(string, "\n")] = '\0';
}

/* ---------------------------------------------------------
   read_string
   ---------------------------------------------------------
   Safely reads a line of text from the user.

   fgets() reads at most (size - 1) characters and adds '\0'.
   We then strip the trailing newline using remove_newline().

   Why fgets over scanf for strings?
   - fgets reads spaces too (scanf stops at whitespace).
   - fgets prevents buffer overflows by respecting 'size'.
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
   ---------------------------------------------------------
   After scanf reads a number, a '\n' stays in the buffer.
   If not cleared, the next read_string() call will pick up
   that '\n' and appear to skip input.

   This function reads and throws away characters until it
   finds '\n' or EOF, effectively emptying the buffer.
   --------------------------------------------------------- */
void clear_input_buffer(void)
{
    int character;

    while ((character = getchar()) != '\n' &&
           character != EOF)
    {
        /* Discard each character until buffer is clean */
    }
}

/* ---------------------------------------------------------
   is_valid_mobile
   ---------------------------------------------------------
   Validates an Indian mobile number:
     - Must be exactly 10 digits long
     - First digit must be 6, 7, 8, or 9
     - Every character must be a digit

   Returns 1 if valid, 0 if invalid.
   --------------------------------------------------------- */
int is_valid_mobile(char mobile[])
{
    int index;
    int length;

    length = strlen(mobile);

    if (length != 10)
    {
        return 0;
    }

    if (mobile[0] != '6' &&
        mobile[0] != '7' &&
        mobile[0] != '8' &&
        mobile[0] != '9')
    {
        return 0;
    }

    for (index = 0; index < length; index++)
    {
        if (!isdigit((unsigned char)mobile[index]))
        {
            return 0;
        }
    }

    return 1;
}

/* ---------------------------------------------------------
   is_valid_email
   ---------------------------------------------------------
   Validates an email address using basic rules:
     - Must contain '@'
     - Must contain '.' after the '@'
     - '@' must not be the first character
     - '.' must not appear before '@'
     - '.' must not be the last character

   Returns 1 if valid, 0 if invalid.

   strchr  → finds the FIRST occurrence of a character
   strrchr → finds the LAST occurrence of a character
   --------------------------------------------------------- */
int is_valid_email(char email[])
{
    char *at_symbol;
    char *dot_symbol;

    at_symbol = strchr(email, '@');
    dot_symbol = strrchr(email, '.');

    if (at_symbol == NULL)
    {
        return 0;
    } /* No '@' found         */
    if (dot_symbol == NULL)
    {
        return 0;
    } /* No '.' found         */
    if (at_symbol == email)
    {
        return 0;
    } /* '@' is first char    */
    if (dot_symbol < at_symbol)
    {
        return 0;
    } /* '.' is before '@'    */
    if (dot_symbol == email + strlen(email) - 1)
    {
        return 0;
    } /* '.' is last char     */

    return 1;
}

/* ---------------------------------------------------------
   phone_already_exists
   ---------------------------------------------------------
   Checks whether a given phone number is already stored
   in another contact.

   The 'ignored_index' parameter lets edit_contact() skip
   the contact currently being edited, so it doesn't flag
   its own existing phone as a duplicate.

   Pass ignored_index = -1 when adding (no contact to skip).

   Returns 1 if duplicate found, 0 if unique.
   --------------------------------------------------------- */
int phone_already_exists(
    struct AddressBook *addressBook,
    char phone[],
    int ignored_index)
{
    int index;

    for (index = 0;
         index < addressBook->contactCount;
         index++)
    {
        if (index == ignored_index)
        {
            continue; /* Skip the contact being edited */
        }

        if (strcmp(addressBook->contacts[index].phone, phone) == 0)
        {
            return 1; /* Duplicate found */
        }
    }

    return 0;
}

/* ---------------------------------------------------------
   email_already_exists
   ---------------------------------------------------------
   Same logic as phone_already_exists but checks email.

   Returns 1 if duplicate found, 0 if unique.
   --------------------------------------------------------- */
int email_already_exists(
    struct AddressBook *addressBook,
    char email[],
    int ignored_index)
{
    int index;

    for (index = 0;
         index < addressBook->contactCount;
         index++)
    {
        if (index == ignored_index)
        {
            continue; /* Skip the contact being edited */
        }

        if (strcmp(addressBook->contacts[index].email, email) == 0)
        {
            return 1; /* Duplicate found */
        }
    }

    return 0;
}