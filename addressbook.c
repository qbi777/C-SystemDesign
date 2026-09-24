#include "contact.h"

/* =========================================================
   MOBILE VERIFY
   Prints validation result for a phone number.
   Uses validate_mobile() to get the specific error reason.
   Called when the user enters a bad phone number.
   ========================================================= */
void mobile_verify(char mobile[])
{
    char error_msg[ERROR_SIZE];

    if (validate_mobile(mobile, error_msg))
    {
        printf("  Mobile number is valid.\n");
    }
    else
    {
        printf("  Error: %s\n", error_msg);
    }
}


/* =========================================================
   GMAIL VERIFY
   Prints validation result for an email address.
   Uses validate_email() to get the specific error reason.
   Called when the user enters a bad email.
   ========================================================= */
void gmail_verify(
    struct AddressBook *addressBook,
    char gmail[])
{
    char error_msg[ERROR_SIZE];

    (void)addressBook;  /* Suppress unused-parameter warning */

    if (validate_email(gmail, error_msg))
    {
        printf("  Email address is valid.\n");
    }
    else
    {
        printf("  Error: %s\n", error_msg);
    }
}


/* =========================================================
   PULL FILE DATA  (Load contacts from CSV)
   Opens database.csv and reads each row with fscanf.

   fscanf format: " %49[^,],%14[^,],%99[^\n]"
     - " "       → skip whitespace / blank lines between rows
     - %49[^,]   → read up to 49 chars, stop at comma  (name)
     - ,         → consume the comma separator
     - %14[^,]   → read up to 14 chars, stop at comma  (phone)
     - ,         → consume the comma separator
     - %99[^\n]  → read up to 99 chars, stop at newline (email)

   If the file does not exist, returns quietly.
   ========================================================= */
void pull_file_data(
    struct AddressBook *addressBook,
    char *file_name)
{
    FILE *file;
    int   fields_read;

    /* Reset before loading */
    addressBook->contactCount = 0;
    addressBook->ir_size      = 0;

    file = fopen(file_name, "r");

    if (file == NULL)
    {
        /* No file yet — will be created on first save */
        return;
    }

    /* Skip the CSV header row: "Name,Phone,Email" */
    fscanf(file, " %*[^\n]");

    /* Read one contact per iteration until EOF or error */
    while (addressBook->contactCount < MAX_CONTACTS)
    {
        fields_read = fscanf(
            file,
            " %49[^,],%14[^,],%99[^\n]",
            addressBook->contacts[addressBook->contactCount].name,
            addressBook->contacts[addressBook->contactCount].phone,
            addressBook->contacts[addressBook->contactCount].email);

        if (fields_read != 3)
        {
            break;  /* End of file or malformed row */
        }

        addressBook->contactCount++;
    }

    fclose(file);
}


/* =========================================================
   PUSH DATA TO FILE  (Save contacts to CSV)
   Opens database.csv in write mode (erases old content)
   and writes all current contacts in CSV format.
   Always writes the header row first.
   ========================================================= */
void push_data_to_file(
    struct AddressBook *addressBook,
    char *file_name)
{
    FILE *file;
    int   index;

    file = fopen(file_name, "w");

    if (file == NULL)
    {
        printf("  Error: Unable to open database file for saving.\n");
        return;
    }

    /* Write CSV header */
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
   INPUT NAME  (internal)
   Prompts the user for a name repeatedly until it passes
   validate_name(). Prints the specific error on each
   failed attempt. Stores result in dest[].
   ========================================================= */
static void input_name(char *prompt, char dest[], int size)
{
    char error_msg[ERROR_SIZE];

    while (1)
    {
        printf("%s", prompt);
        read_string(dest, size);

        if (strlen(dest) == 0)
        {
            printf("  Error: Name cannot be empty.\n");
            continue;
        }

        if (!validate_name(dest, error_msg))
        {
            printf("  Error: %s\n", error_msg);
            continue;
        }

        break;  /* Valid — exit loop */
    }
}


/* =========================================================
   INPUT PHONE  (internal)
   Prompts the user for a phone number repeatedly until it
   passes validate_mobile() AND is not a duplicate.
   ignored_index = -1 for add, or the contact's own index
   for edit (so it does not flag itself as duplicate).
   ========================================================= */
static void input_phone(
    char *prompt,
    char dest[],
    int size,
    struct AddressBook *addressBook,
    int ignored_index)
{
    char error_msg[ERROR_SIZE];

    while (1)
    {
        printf("%s", prompt);
        read_string(dest, size);

        if (strlen(dest) == 0)
        {
            printf("  Error: Phone number cannot be empty.\n");
            continue;
        }

        if (!validate_mobile(dest, error_msg))
        {
            printf("  Error: %s\n", error_msg);
            continue;
        }

        if (phone_already_exists(addressBook, dest, ignored_index))
        {
            printf("  Error: This phone number already exists. Use a different number.\n");
            continue;
        }

        break;  /* Valid and unique — exit loop */
    }
}


/* =========================================================
   INPUT EMAIL  (internal)
   Prompts the user for an email repeatedly until it passes
   validate_email() AND is not a duplicate.
   ignored_index = -1 for add, or the contact's own index
   for edit (so it does not flag itself as duplicate).
   ========================================================= */
static void input_email(
    char *prompt,
    char dest[],
    int size,
    struct AddressBook *addressBook,
    int ignored_index)
{
    char error_msg[ERROR_SIZE];

    while (1)
    {
        printf("%s", prompt);
        read_string(dest, size);

        if (strlen(dest) == 0)
        {
            printf("  Error: Email cannot be empty.\n");
            continue;
        }

        if (!validate_email(dest, error_msg))
        {
            printf("  Error: %s\n", error_msg);
            continue;
        }

        if (email_already_exists(addressBook, dest, ignored_index))
        {
            printf("  Error: This email already exists. Use a different email.\n");
            continue;
        }

        break;  /* Valid and unique — exit loop */
    }
}


/* =========================================================
   ADD CONTACT
   Collects name, phone, and email from the user.
   Each field is validated and duplicate-checked before
   the contact is stored and the CSV is updated.
   ========================================================= */
void add_contact(
    struct AddressBook *addressBook)
{
    struct Contact new_contact;

    /* Check capacity before collecting any input */
    if (addressBook->contactCount >= MAX_CONTACTS)
    {
        printf("\n  Address Book is full. Cannot add more contacts.\n");
        return;
    }

    printf("\n");
    printf("====================================\n");
    printf("           ADD CONTACT\n");
    printf("====================================\n");

    /* Collect and validate name */
    input_name(
        "  Enter name  : ",
        new_contact.name,
        NAME_SIZE);

    /* Collect and validate phone (no duplicate check index: -1) */
    input_phone(
        "  Enter phone : ",
        new_contact.phone,
        PHONE_SIZE,
        addressBook,
        -1);

    /* Collect and validate email (no duplicate check index: -1) */
    input_email(
        "  Enter email : ",
        new_contact.email,
        EMAIL_SIZE,
        addressBook,
        -1);

    /* Store the contact */
    addressBook->contacts[addressBook->contactCount] = new_contact;
    addressBook->contactCount++;

    /* Save immediately to CSV */
    push_data_to_file(addressBook, FILE_NAME);

    printf("\n  Contact added successfully!\n");
}


/* =========================================================
   LIST CONTACTS
   Displays every contact as a numbered table with columns
   for name, phone, and email. Shows total count at the end.
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
        printf("  No contacts available.\n");
        return;
    }

    /* Table header row */
    printf(
        "  %-5s %-22s %-13s %-35s\n",
        "No.", "Name", "Phone", "Email");

    printf(
        "  ------------------------------------------------------------------\n");

    /* One contact per row */
    for (index = 0; index < addressBook->contactCount; index++)
    {
        printf(
            "  %-5d %-22s %-13s %-35s\n",
            index + 1,
            addressBook->contacts[index].name,
            addressBook->contacts[index].phone,
            addressBook->contacts[index].email);
    }

    printf(
        "\n  Total contacts: %d\n",
        addressBook->contactCount);
}


/* =========================================================
   DISPLAY SEARCH RESULTS  (internal)
   Prints all contacts stored in index_record[] as a
   numbered list. Called after any search operation.
   'label' is the heading shown above the results.
   ========================================================= */
static void display_search_results(
    struct AddressBook *addressBook,
    const char *label)
{
    int index;

    printf("\n  %s (%d found):\n", label, addressBook->ir_size);
    printf("  ------------------------------------------------------------------\n");

    for (index = 0; index < addressBook->ir_size; index++)
    {
        int ci = addressBook->index_record[index];  /* Shorthand for contact index */

        printf(
            "\n  [%d] Name  : %s\n"
            "      Phone : %s\n"
            "      Email : %s\n",
            index + 1,
            addressBook->contacts[ci].name,
            addressBook->contacts[ci].phone,
            addressBook->contacts[ci].email);
    }

    printf("  ------------------------------------------------------------------\n");
}


/* =========================================================
   SEARCH BY NAME
   Finds every contact whose name CONTAINS the search string.
   Uses strstr() so partial matches work (e.g. "Ali" matches
   "Alice" and "Malik"). Fills index_record[] with results.
   ========================================================= */
void search_by_name(
    struct AddressBook *addressBook,
    char name[])
{
    int index;

    addressBook->ir_size = 0;  /* Clear previous results */

    for (index = 0; index < addressBook->contactCount; index++)
    {
        /* strstr returns NULL if no match, non-NULL if found */
        if (strstr(addressBook->contacts[index].name, name) != NULL)
        {
            addressBook->index_record[addressBook->ir_size] = index;
            addressBook->ir_size++;
        }
    }

    if (addressBook->ir_size == 0)
    {
        printf("\n  No contact found with name containing \"%s\".\n", name);
        return;
    }

    display_search_results(addressBook, "Search Results");
}


/* =========================================================
   SEARCH BY PHONE
   Finds every contact whose phone CONTAINS the search string.
   Partial matches are supported (e.g. "987" matches any
   number containing those digits). Fills index_record[].
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
        printf("\n  No contact found with phone containing \"%s\".\n", phone);
        return;
    }

    display_search_results(addressBook, "Search Results");
}


/* =========================================================
   SEARCH BY EMAIL
   Finds every contact whose email CONTAINS the search string.
   Partial matches supported. Fills index_record[].
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
        printf("\n  No contact found with email containing \"%s\".\n", gmail);
        return;
    }

    display_search_results(addressBook, "Search Results");
}


/* =========================================================
   SEARCH CONTACT  (Menu-driven)
   Presents a search sub-menu (Name / Phone / Email / Exit).
   Reads the search term and delegates to the right function.
   All matching contacts are displayed, not just the first.
   ========================================================= */
void search_contact(
    struct AddressBook *addressBook)
{
    int  choice;
    char search_value[EMAIL_SIZE];

    if (addressBook->contactCount == 0)
    {
        printf("\n  No contacts available to search.\n");
        return;
    }

    while (1)
    {
        printf("\n");
        printf("====================================\n");
        printf("          SEARCH CONTACT\n");
        printf("====================================\n");
        printf("  1. Search by Name\n");
        printf("  2. Search by Phone\n");
        printf("  3. Search by Email\n");
        printf("  4. Exit Search\n");
        printf("====================================\n");
        printf("  Enter choice: ");

        if (scanf("%d", &choice) != 1)
        {
            printf("  Error: Invalid input. Please enter a number (1-4).\n");
            clear_input_buffer();
            continue;
        }

        clear_input_buffer();

        switch (choice)
        {
            case 1:
                printf("  Enter name to search: ");
                read_string(search_value, EMAIL_SIZE);
                search_by_name(addressBook, search_value);
                break;

            case 2:
                printf("  Enter phone to search: ");
                read_string(search_value, EMAIL_SIZE);
                search_by_phone(addressBook, search_value);
                break;

            case 3:
                printf("  Enter email to search: ");
                read_string(search_value, EMAIL_SIZE);
                search_by_gmail(addressBook, search_value);
                break;

            case 4:
                printf("  Returning to main menu.\n");
                return;  /* Exit the search menu */

            default:
                printf("  Error: Invalid choice. Please enter a number between 1 and 4.\n");
        }
    }
}


/* =========================================================
   PICK CONTACT FROM RESULTS  (internal)
   After a search fills index_record[], this function asks
   the user to choose ONE contact by number.

   - If only 1 match: selects it automatically.
   - If multiple: asks the user to type a number.
   - 0 cancels the operation.

   Returns the contacts[] index of the chosen contact,
   or -1 if cancelled or invalid input.
   ========================================================= */
static int pick_contact_from_results(
    struct AddressBook *addressBook)
{
    int pick;

    if (addressBook->ir_size == 0)
    {
        return -1;
    }

    /* Single match — no need to ask */
    if (addressBook->ir_size == 1)
    {
        printf("  1 contact found. Selecting automatically.\n");
        return addressBook->index_record[0];
    }

    /* Multiple matches — ask the user to pick */
    while (1)
    {
        printf("\n  Enter the number of the contact (1-%d), or 0 to cancel: ",
               addressBook->ir_size);

        if (scanf("%d", &pick) != 1)
        {
            printf("  Error: Invalid input. Please enter a number.\n");
            clear_input_buffer();
            continue;
        }

        clear_input_buffer();

        if (pick == 0)
        {
            printf("  Operation cancelled.\n");
            return -1;
        }

        if (pick < 1 || pick > addressBook->ir_size)
        {
            printf("  Error: Please enter a number between 1 and %d, or 0 to cancel.\n",
                   addressBook->ir_size);
            continue;
        }

        /* Convert 1-based user pick to 0-based array index */
        return addressBook->index_record[pick - 1];
    }
}


/* =========================================================
   RUN SEARCH MENU  (internal)
   The shared search sub-menu used by both edit_contact()
   and delete_contact(). Presents Name / Phone / Email / Exit
   options, runs the chosen search, and fills index_record[].

   'action_label' is displayed in the heading (e.g. "EDIT").
   Returns the number of matches found, or -1 if the user
   chose Exit.
   ========================================================= */
static int run_search_menu(
    struct AddressBook *addressBook,
    const char *action_label)
{
    int  choice;
    char search_value[EMAIL_SIZE];

    while (1)
    {
        printf("\n");
        printf("====================================\n");
        printf("    Search contact to %s\n", action_label);
        printf("====================================\n");
        printf("  1. Search by Name\n");
        printf("  2. Search by Phone\n");
        printf("  3. Search by Email\n");
        printf("  4. Exit\n");
        printf("====================================\n");
        printf("  Enter choice: ");

        if (scanf("%d", &choice) != 1)
        {
            printf("  Error: Invalid input. Please enter a number (1-4).\n");
            clear_input_buffer();
            continue;
        }

        clear_input_buffer();

        switch (choice)
        {
            case 1:
                printf("  Enter name: ");
                read_string(search_value, EMAIL_SIZE);
                search_by_name(addressBook, search_value);
                break;

            case 2:
                printf("  Enter phone: ");
                read_string(search_value, EMAIL_SIZE);
                search_by_phone(addressBook, search_value);
                break;

            case 3:
                printf("  Enter email: ");
                read_string(search_value, EMAIL_SIZE);
                search_by_gmail(addressBook, search_value);
                break;

            case 4:
                printf("  Returning to main menu.\n");
                return -1;  /* Signal: user chose Exit */

            default:
                printf("  Error: Invalid choice. Please enter a number between 1 and 4.\n");
                continue;
        }

        /* A valid search case ran — return the match count */
        return addressBook->ir_size;
    }
}


/* =========================================================
   EDIT CONTACT
   Flow:
     1. Run search menu so user finds the contact.
     2. List all matches; user picks one by number.
     3. Show current details.
     4. For each field: press Enter to keep, or type new value.
        New values are re-validated with the same rules as Add.
     5. Save to CSV.
   ========================================================= */
void edit_contact(
    struct AddressBook *addressBook)
{
    int  contact_index;
    int  search_result;
    char new_name[NAME_SIZE];
    char new_phone[PHONE_SIZE];
    char new_email[EMAIL_SIZE];
    char error_msg[ERROR_SIZE];

    if (addressBook->contactCount == 0)
    {
        printf("\n  No contacts available to edit.\n");
        return;
    }

    /* Step 1: Search */
    search_result = run_search_menu(addressBook, "EDIT");

    if (search_result == -1 || search_result == 0)
    {
        return;  /* User exited or no matches found */
    }

    /* Step 2: Pick one contact from the results */
    contact_index = pick_contact_from_results(addressBook);

    if (contact_index == -1)
    {
        return;  /* Cancelled */
    }

    /* Step 3: Show current details */
    printf("\n  --- Current Details ---\n");
    printf("  Name  : %s\n", addressBook->contacts[contact_index].name);
    printf("  Phone : %s\n", addressBook->contacts[contact_index].phone);
    printf("  Email : %s\n", addressBook->contacts[contact_index].email);
    printf("  -----------------------\n");
    printf("  (Press Enter on any field to keep the current value)\n\n");

    /* Step 4a: Edit name */
    while (1)
    {
        printf("  New name  [%s]: ", addressBook->contacts[contact_index].name);
        read_string(new_name, NAME_SIZE);

        if (strlen(new_name) == 0)
        {
            break;  /* Keep existing name */
        }

        if (!validate_name(new_name, error_msg))
        {
            printf("  Error: %s\n", error_msg);
            continue;
        }

        strcpy(addressBook->contacts[contact_index].name, new_name);
        break;
    }

    /* Step 4b: Edit phone */
    while (1)
    {
        printf("  New phone [%s]: ", addressBook->contacts[contact_index].phone);
        read_string(new_phone, PHONE_SIZE);

        if (strlen(new_phone) == 0)
        {
            break;  /* Keep existing phone */
        }

        if (!validate_mobile(new_phone, error_msg))
        {
            printf("  Error: %s\n", error_msg);
            continue;
        }

        if (phone_already_exists(addressBook, new_phone, contact_index))
        {
            printf("  Error: This phone number already exists. Use a different number.\n");
            continue;
        }

        strcpy(addressBook->contacts[contact_index].phone, new_phone);
        break;
    }

    /* Step 4c: Edit email */
    while (1)
    {
        printf("  New email [%s]: ", addressBook->contacts[contact_index].email);
        read_string(new_email, EMAIL_SIZE);

        if (strlen(new_email) == 0)
        {
            break;  /* Keep existing email */
        }

        if (!validate_email(new_email, error_msg))
        {
            printf("  Error: %s\n", error_msg);
            continue;
        }

        if (email_already_exists(addressBook, new_email, contact_index))
        {
            printf("  Error: This email already exists. Use a different email.\n");
            continue;
        }

        strcpy(addressBook->contacts[contact_index].email, new_email);
        break;
    }

    /* Step 5: Save */
    push_data_to_file(addressBook, FILE_NAME);

    printf("\n  Contact updated successfully!\n");
}


/* =========================================================
   DELETE CONTACT
   Flow:
     1. Run search menu so user finds the contact.
     2. List all matches; user picks one by number.
     3. Show the selected contact and ask for confirmation.
     4. Shift the contacts array left to close the gap.
     5. Save to CSV.
   ========================================================= */
void delete_contact(
    struct AddressBook *addressBook)
{
    int  contact_index;
    int  search_result;
    int  index;
    char confirmation;

    if (addressBook->contactCount == 0)
    {
        printf("\n  No contacts available to delete.\n");
        return;
    }

    /* Step 1: Search */
    search_result = run_search_menu(addressBook, "DELETE");

    if (search_result == -1 || search_result == 0)
    {
        return;  /* User exited or no matches found */
    }

    /* Step 2: Pick one contact from the results */
    contact_index = pick_contact_from_results(addressBook);

    if (contact_index == -1)
    {
        return;  /* Cancelled */
    }

    /* Step 3: Show selected contact and confirm */
    printf("\n  Contact selected for deletion:\n");
    printf("  Name  : %s\n", addressBook->contacts[contact_index].name);
    printf("  Phone : %s\n", addressBook->contacts[contact_index].phone);
    printf("  Email : %s\n", addressBook->contacts[contact_index].email);

    while (1)
    {
        printf("\n  Are you sure you want to delete this contact? (y/n): ");

        scanf(" %c", &confirmation);
        clear_input_buffer();

        if (confirmation == 'y' || confirmation == 'Y')
        {
            break;  /* Confirmed */
        }

        if (confirmation == 'n' || confirmation == 'N')
        {
            printf("  Delete cancelled.\n");
            return;
        }

        /* Neither y nor n */
        printf("  Error: Please enter 'y' to confirm or 'n' to cancel.\n");
    }

    /* Step 4: Shift array left to close the gap */
    for (index = contact_index;
         index < addressBook->contactCount - 1;
         index++)
    {
        /* Copy the next contact into the current slot */
        addressBook->contacts[index] = addressBook->contacts[index + 1];
    }

    addressBook->contactCount--;

    /* Step 5: Save */
    push_data_to_file(addressBook, FILE_NAME);

    printf("\n  Contact deleted successfully!\n");
}
