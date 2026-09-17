#include "contact.h"

/* =========================================================
   MOBILE VERIFICATION
   =========================================================
   Calls is_valid_mobile() and prints a human-readable
   message telling the user whether the number is valid.
   ========================================================= */

void mobile_verify(char mobile[])
{
    if (is_valid_mobile(mobile))
    {
        printf("Mobile number is valid.\n");
    }
    else
    {
        printf(
            "Invalid mobile number.\n"
            "Enter a 10-digit number starting with 6, 7, 8 or 9.\n");
    }
}

/* =========================================================
   GMAIL VERIFICATION
   =========================================================
   Calls is_valid_email() and prints a human-readable result.
   The addressBook parameter is kept for consistency with the
   header declaration; it is not used here.
   ========================================================= */

void gmail_verify(
    struct AddressBook *addressBook,
    char gmail[])
{
    (void)addressBook; /* Suppress unused-parameter warning */

    if (is_valid_email(gmail))
    {
        printf("Email address is valid.\n");
    }
    else
    {
        printf("Invalid email address.\n");
    }
}

/* =========================================================
   PULL FILE DATA  (Load contacts from CSV)
   =========================================================
   Opens database.csv and reads every row into the
   addressBook->contacts[] array using fscanf.

   fscanf format:  " %49[^,],%14[^,],%49[^\n]"
     - " "       → skip any leading whitespace or newlines
     - %49[^,]   → read up to 49 chars, stop at comma  (name)
     - ,         → consume the comma separator
     - %14[^,]   → read up to 14 chars, stop at comma  (phone)
     - ,         → consume the comma separator
     - %49[^\n]  → read up to 49 chars, stop at newline (email)

   If the file does not exist yet, the function returns
   quietly. The file is created the first time a contact
   is saved.
   ========================================================= */

void pull_file_data(
    struct AddressBook *addressBook,
    char *file_name)
{
    FILE *file;

    /* Reset counts before loading */
    addressBook->contactCount = 0;
    addressBook->ir_size = 0;

    file = fopen(file_name, "r");

    if (file == NULL)
    {
        /* File does not exist yet — that is fine */
        return;
    }

    /*
       Skip the CSV header line ("Name,Phone,Email").
       fscanf with %*[^\n] reads and discards until newline.
       The second fscanf with %*c discards the newline itself.
    */
    fscanf(file, " %*[^\n]");

    /*
       Read one contact per loop iteration.
       fscanf returns the number of fields successfully read.
       We expect 3 (name, phone, email); stop if fewer.
    */
    while (addressBook->contactCount < MAX_CONTACTS)
    {
        int fields_read = fscanf(
            file,
            " %49[^,],%14[^,],%49[^\n]",
            addressBook->contacts[addressBook->contactCount].name,
            addressBook->contacts[addressBook->contactCount].phone,
            addressBook->contacts[addressBook->contactCount].email);

        if (fields_read != 3)
        {
            /* End of file or malformed row — stop reading */
            break;
        }

        addressBook->contactCount++;
    }

    fclose(file);
}

/* =========================================================
   PUSH DATA TO FILE  (Save contacts to CSV)
   =========================================================
   Opens database.csv in write mode ("w"), which creates
   the file if it does not exist and overwrites it if it does.
   Writes all contacts in CSV format.
   ========================================================= */

void push_data_to_file(
    struct AddressBook *addressBook,
    char *file_name)
{
    FILE *file;
    int index;

    file = fopen(file_name, "w");

    if (file == NULL)
    {
        printf("Error: Unable to open database file for saving.\n");
        return;
    }

    /* Write CSV header row */
    fprintf(file, "Name,Phone,Email\n");

    /* Write one contact per line */
    for (index = 0; index < addressBook->contactCount; index++)
    {
        fprintf(
            file,
            "%s,%s,%s\n",
            addressBook->contacts[index].name,
            addressBook->contacts[index].phone,
            addressBook->contacts[index].email);
    }

    fclose(file);
}

/* =========================================================
   ADD CONTACT
   =========================================================
   Collects name, phone, and email from the user.
   Validates each field and checks for duplicates before
   storing the new contact and saving to the CSV file.
   ========================================================= */

void add_contact(
    struct AddressBook *addressBook)
{
    struct Contact new_contact;

    if (addressBook->contactCount >= MAX_CONTACTS)
    {
        printf("\nAddress Book is full. Cannot add more contacts.\n");
        return;
    }

    printf("\n");
    printf("====================================\n");
    printf("           ADD CONTACT\n");
    printf("====================================\n");

    /* -------------------------------------------------
       NAME
       Enter a non-empty name.
       ------------------------------------------------- */

    while (1)
    {
        printf("Enter name : ");

        read_string(new_contact.name, NAME_SIZE);

        if (strlen(new_contact.name) == 0)
        {
            printf("Name cannot be empty. Please try again.\n");
            continue;
        }

        break;
    }

    /* -------------------------------------------------
       PHONE
       Must be a valid Indian mobile number and not
       already exist in the address book.
       ------------------------------------------------- */

    while (1)
    {
        printf("Enter phone: ");

        read_string(new_contact.phone, PHONE_SIZE);

        if (!is_valid_mobile(new_contact.phone))
        {
            mobile_verify(new_contact.phone);
            continue;
        }

        if (phone_already_exists(addressBook, new_contact.phone, -1))
        {
            printf("This phone number already exists. Use a different number.\n");
            continue;
        }

        break;
    }

    /* -------------------------------------------------
       EMAIL
       Must be a valid email format and not already
       exist in the address book.
       ------------------------------------------------- */

    while (1)
    {
        printf("Enter email: ");

        read_string(new_contact.email, EMAIL_SIZE);

        if (!is_valid_email(new_contact.email))
        {
            gmail_verify(addressBook, new_contact.email);
            continue;
        }

        if (email_already_exists(addressBook, new_contact.email, -1))
        {
            printf("This email already exists. Use a different email.\n");
            continue;
        }

        break;
    }

    /* Store the new contact and increment count */
    addressBook->contacts[addressBook->contactCount] = new_contact;
    addressBook->contactCount++;

    /* Immediately persist to CSV */
    push_data_to_file(addressBook, FILE_NAME);

    printf("\nContact added successfully!\n");
}

/* =========================================================
   LIST CONTACTS
   =========================================================
   Displays every contact in a formatted table with
   a serial number, name, phone, and email column.
   ========================================================= */

void list_contacts(
    struct AddressBook *addressBook)
{
    int index;

    printf("\n");
    printf("====================================================================\n");
    printf("                         CONTACT LIST\n");
    printf("====================================================================\n");

    if (addressBook->contactCount == 0)
    {
        printf("No contacts available.\n");
        return;
    }

    /* Table header */
    printf(
        "%-5s %-25s %-15s %-30s\n",
        "No.",
        "Name",
        "Phone",
        "Email");

    printf(
        "--------------------------------------------------------------------\n");

    /* One row per contact */
    for (index = 0; index < addressBook->contactCount; index++)
    {
        printf(
            "%-5d %-25s %-15s %-30s\n",
            index + 1,
            addressBook->contacts[index].name,
            addressBook->contacts[index].phone,
            addressBook->contacts[index].email);
    }

    printf(
        "\nTotal contacts: %d\n",
        addressBook->contactCount);
}

/* =========================================================
   DISPLAY SEARCH RESULTS  (internal helper)
   =========================================================
   Prints all contacts currently stored in index_record[].
   The 'label' parameter is a heading shown above the list
   (e.g. "Search Results", "Select a contact to edit").

   Returns the ir_size so callers know how many were shown.
   ========================================================= */

static void display_search_results(
    struct AddressBook *addressBook,
    const char *label)
{
    int index;

    printf("\n%s:\n", label);
    printf("--------------------------------------------------------------------\n");

    for (index = 0; index < addressBook->ir_size; index++)
    {
        int ci = addressBook->index_record[index]; /* contact index shorthand */

        printf(
            "\n[%d] Name  : %s\n"
            "    Phone : %s\n"
            "    Email : %s\n",
            index + 1,
            addressBook->contacts[ci].name,
            addressBook->contacts[ci].phone,
            addressBook->contacts[ci].email);
    }

    printf("--------------------------------------------------------------------\n");
}

/* =========================================================
   SEARCH BY NAME
   =========================================================
   Stores in index_record[] the indices of every contact
   whose name CONTAINS the search string (case-sensitive).
   Uses strstr() so partial matches work too.
   ========================================================= */

void search_by_name(
    struct AddressBook *addressBook,
    char name[])
{
    int index;

    /* Clear previous search results */
    addressBook->ir_size = 0;

    for (index = 0; index < addressBook->contactCount; index++)
    {
        /*
           strstr returns a pointer to the first occurrence
           of 'name' inside the contact's name, or NULL if
           no match. NULL means no match; non-NULL means found.
        */
        if (strstr(addressBook->contacts[index].name, name) != NULL)
        {
            addressBook->index_record[addressBook->ir_size] = index;
            addressBook->ir_size++;
        }
    }

    if (addressBook->ir_size == 0)
    {
        printf("\nNo contact found with the name containing \"%s\".\n", name);
        return;
    }

    display_search_results(addressBook, "Search Results");
}

/* =========================================================
   SEARCH BY PHONE
   =========================================================
   Same logic as search_by_name but matches against the
   phone field. Partial phone matches are supported.
   ========================================================= */

void search_by_phone(
    struct AddressBook *addressBook,
    char phone[])
{
    int index;

    addressBook->ir_size = 0;

    for (index = 0; index < addressBook->contactCount; index++)
    {
        if (strstr(addressBook->contacts[index].phone, phone) != NULL)
        {
            addressBook->index_record[addressBook->ir_size] = index;
            addressBook->ir_size++;
        }
    }

    if (addressBook->ir_size == 0)
    {
        printf("\nNo contact found with the phone containing \"%s\".\n", phone);
        return;
    }

    display_search_results(addressBook, "Search Results");
}

/* =========================================================
   SEARCH BY EMAIL
   =========================================================
   Same logic as search_by_name but matches against the
   email field. Partial email matches are supported.
   ========================================================= */

void search_by_gmail(
    struct AddressBook *addressBook,
    char gmail[])
{
    int index;

    addressBook->ir_size = 0;

    for (index = 0; index < addressBook->contactCount; index++)
    {
        if (strstr(addressBook->contacts[index].email, gmail) != NULL)
        {
            addressBook->index_record[addressBook->ir_size] = index;
            addressBook->ir_size++;
        }
    }

    if (addressBook->ir_size == 0)
    {
        printf("\nNo contact found with the email containing \"%s\".\n", gmail);
        return;
    }

    display_search_results(addressBook, "Search Results");
}

/* =========================================================
   SEARCH CONTACT  (Menu-driven search)
   =========================================================
   Asks the user HOW they want to search, reads the search
   term, and calls the appropriate search_by_*() function.
   All matches are displayed (not just the first one).
   ========================================================= */

void search_contact(
    struct AddressBook *addressBook)
{
    int choice;
    char search_value[EMAIL_SIZE];

    if (addressBook->contactCount == 0)
    {
        printf("\nNo contacts available.\n");
        return;
    }

    printf("\n");
    printf("====================================\n");
    printf("          SEARCH CONTACT\n");
    printf("====================================\n");

    printf("1. Search by Name\n");
    printf("2. Search by Phone\n");
    printf("3. Search by Email\n");
    printf("Enter choice: ");

    if (scanf("%d", &choice) != 1)
    {
        printf("Invalid input.\n");
        clear_input_buffer();
        return;
    }

    clear_input_buffer();

    switch (choice)
    {
    case 1:
        printf("Enter name to search: ");
        read_string(search_value, EMAIL_SIZE);
        search_by_name(addressBook, search_value);
        break;

    case 2:
        printf("Enter phone to search: ");
        read_string(search_value, EMAIL_SIZE);
        search_by_phone(addressBook, search_value);
        break;

    case 3:
        printf("Enter email to search: ");
        read_string(search_value, EMAIL_SIZE);
        search_by_gmail(addressBook, search_value);
        break;

    default:
        printf("Invalid search choice.\n");
    }
}

/* =========================================================
   PICK CONTACT FROM SEARCH  (internal helper)
   =========================================================
   After a search fills index_record[], this function asks
   the user to pick ONE contact from the list by number.

   Returns the actual contacts[] array index of the chosen
   contact, or -1 if the user cancelled or gave bad input.

   This is reused by both edit_contact() and delete_contact()
   to avoid repeating the same pick-logic twice.
   ========================================================= */

static int pick_contact_from_results(
    struct AddressBook *addressBook)
{
    int pick;

    if (addressBook->ir_size == 0)
    {
        return -1;
    }

    if (addressBook->ir_size == 1)
    {
        /*
           Only one match — confirm automatically.
           No need to ask the user to pick.
        */
        printf("1 contact found. Selecting it automatically.\n");
        return addressBook->index_record[0];
    }

    /* Multiple matches — ask user to choose */
    printf("\nEnter the number of the contact (1 to %d), or 0 to cancel: ",
           addressBook->ir_size);

    if (scanf("%d", &pick) != 1)
    {
        printf("Invalid input.\n");
        clear_input_buffer();
        return -1;
    }

    clear_input_buffer();

    if (pick == 0)
    {
        printf("Operation cancelled.\n");
        return -1;
    }

    if (pick < 1 || pick > addressBook->ir_size)
    {
        printf("Invalid number. Operation cancelled.\n");
        return -1;
    }

    /* Convert 1-based user choice to 0-based array index */
    return addressBook->index_record[pick - 1];
}

/* =========================================================
   RUN SEARCH FOR EDIT / DELETE  (internal helper)
   =========================================================
   Presents the same search-by-name/phone/email menu and
   populates index_record[] with all matching contacts.
   Called at the start of edit_contact() and delete_contact()
   so both operations use the same unified search flow.

   Returns the number of matches found (ir_size).
   ========================================================= */

static int run_search_menu(
    struct AddressBook *addressBook,
    const char *action_label)
{
    int choice;
    char search_value[EMAIL_SIZE];

    printf("\n");
    printf("====================================\n");
    printf("   Search contact to %s\n", action_label);
    printf("====================================\n");

    printf("1. Search by Name\n");
    printf("2. Search by Phone\n");
    printf("3. Search by Email\n");
    printf("Enter choice: ");

    if (scanf("%d", &choice) != 1)
    {
        printf("Invalid input.\n");
        clear_input_buffer();
        return 0;
    }

    clear_input_buffer();

    switch (choice)
    {
    case 1:
        printf("Enter name: ");
        read_string(search_value, EMAIL_SIZE);
        search_by_name(addressBook, search_value);
        break;

    case 2:
        printf("Enter phone: ");
        read_string(search_value, EMAIL_SIZE);
        search_by_phone(addressBook, search_value);
        break;

    case 3:
        printf("Enter email: ");
        read_string(search_value, EMAIL_SIZE);
        search_by_gmail(addressBook, search_value);
        break;

    default:
        printf("Invalid choice.\n");
        return 0;
    }

    return addressBook->ir_size;
}

/* =========================================================
   EDIT CONTACT
   =========================================================
   Flow:
     1. Run the search menu (same as Search Contact).
     2. Display all matching contacts with numbers.
     3. Ask user to pick one contact by number.
     4. Show current details and let user enter new values.
     5. Validate new phone and email (no duplicates).
     6. Save updated data to CSV.

   This handles cases where multiple contacts share the
   same name — the user picks exactly which one to edit.
   ========================================================= */

void edit_contact(
    struct AddressBook *addressBook)
{
    int contact_index;

    if (addressBook->contactCount == 0)
    {
        printf("\nNo contacts available.\n");
        return;
    }

    /* Step 1: Search */
    if (run_search_menu(addressBook, "EDIT") == 0)
    {
        return; /* No matches found */
    }

    /* Step 2 & 3: Show results and pick one */
    contact_index = pick_contact_from_results(addressBook);

    if (contact_index == -1)
    {
        return; /* User cancelled or bad input */
    }

    /* Step 4: Show current details */
    printf("\n--- Current Details ---\n");
    printf("Name  : %s\n", addressBook->contacts[contact_index].name);
    printf("Phone : %s\n", addressBook->contacts[contact_index].phone);
    printf("Email : %s\n", addressBook->contacts[contact_index].email);
    printf("-----------------------\n");

    /* -------------------------------------------------
       EDIT NAME
       If the user just presses Enter (empty string),
       keep the existing name unchanged.
       ------------------------------------------------- */

    {
        char new_name[NAME_SIZE];

        printf("\nEnter new name (press Enter to keep \"%s\"): ",
               addressBook->contacts[contact_index].name);

        read_string(new_name, NAME_SIZE);

        if (strlen(new_name) > 0)
        {
            strcpy(addressBook->contacts[contact_index].name, new_name);
        }
    }

    /* -------------------------------------------------
       EDIT PHONE
       Must be valid and not belong to another contact.
       Press Enter to keep existing.
       ------------------------------------------------- */

    while (1)
    {
        char new_phone[PHONE_SIZE];

        printf("Enter new phone (press Enter to keep \"%s\"): ",
               addressBook->contacts[contact_index].phone);

        read_string(new_phone, PHONE_SIZE);

        if (strlen(new_phone) == 0)
        {
            /* Keep existing phone */
            break;
        }

        if (!is_valid_mobile(new_phone))
        {
            mobile_verify(new_phone);
            continue;
        }

        if (phone_already_exists(addressBook, new_phone, contact_index))
        {
            printf("Phone number already belongs to another contact.\n");
            continue;
        }

        strcpy(addressBook->contacts[contact_index].phone, new_phone);
        break;
    }

    /* -------------------------------------------------
       EDIT EMAIL
       Must be valid and not belong to another contact.
       Press Enter to keep existing.
       ------------------------------------------------- */

    while (1)
    {
        char new_email[EMAIL_SIZE];

        printf("Enter new email (press Enter to keep \"%s\"): ",
               addressBook->contacts[contact_index].email);

        read_string(new_email, EMAIL_SIZE);

        if (strlen(new_email) == 0)
        {
            /* Keep existing email */
            break;
        }

        if (!is_valid_email(new_email))
        {
            gmail_verify(addressBook, new_email);
            continue;
        }

        if (email_already_exists(addressBook, new_email, contact_index))
        {
            printf("Email already belongs to another contact.\n");
            continue;
        }

        strcpy(addressBook->contacts[contact_index].email, new_email);
        break;
    }

    /* Save changes */
    push_data_to_file(addressBook, FILE_NAME);

    printf("\nContact updated successfully!\n");
}

/* =========================================================
   DELETE CONTACT
   =========================================================
   Flow:
     1. Run the search menu (same as Search Contact).
     2. Display all matching contacts with numbers.
     3. Ask user to pick one contact by number.
     4. Show the chosen contact and ask for confirmation.
     5. Shift remaining contacts left to fill the gap.
     6. Save updated data to CSV.

   This handles cases where multiple contacts share the
   same name — the user picks exactly which one to delete.
   ========================================================= */

void delete_contact(
    struct AddressBook *addressBook)
{
    int contact_index;
    int index;
    char confirmation;

    if (addressBook->contactCount == 0)
    {
        printf("\nNo contacts available.\n");
        return;
    }

    /* Step 1: Search */
    if (run_search_menu(addressBook, "DELETE") == 0)
    {
        return; /* No matches found */
    }

    /* Step 2 & 3: Show results and pick one */
    contact_index = pick_contact_from_results(addressBook);

    if (contact_index == -1)
    {
        return; /* User cancelled or bad input */
    }

    /* Step 4: Show chosen contact and ask confirmation */
    printf("\nContact to delete:\n");
    printf("Name  : %s\n", addressBook->contacts[contact_index].name);
    printf("Phone : %s\n", addressBook->contacts[contact_index].phone);
    printf("Email : %s\n", addressBook->contacts[contact_index].email);

    printf("\nAre you sure you want to delete this contact? (y/n): ");

    scanf(" %c", &confirmation);
    clear_input_buffer();

    if (confirmation != 'y' && confirmation != 'Y')
    {
        printf("Delete cancelled.\n");
        return;
    }

    /* Step 5: Shift contacts left to fill the gap left by deletion */
    for (index = contact_index;
         index < addressBook->contactCount - 1;
         index++)
    {
        /*
           Copy the next contact into the current slot.
           This overwrites the deleted contact and closes
           the gap without leaving an empty hole in the array.
        */
        addressBook->contacts[index] = addressBook->contacts[index + 1];
    }

    addressBook->contactCount--;

    /* Step 6: Save */
    push_data_to_file(addressBook, FILE_NAME);

    printf("\nContact deleted successfully!\n");
}