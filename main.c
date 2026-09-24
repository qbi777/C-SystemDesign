#include "contact.h"

/* =========================================================
   MAIN
   Entry point of the Address Book application.
   Responsibilities:
     1. Initialise the AddressBook struct.
     2. Load existing contacts from database.csv.
     3. Display the main menu in a loop.
     4. Dispatch to the correct operation based on user input.
     5. Save contacts to CSV before exiting.
   ========================================================= */
int main(void)
{
    struct AddressBook addressBook;
    int choice;

    /* Initialise the address book */
    addressBook.contactCount = 0;
    addressBook.ir_size      = 0;

    /* Load saved contacts from database.csv */
    pull_file_data(&addressBook, FILE_NAME);

    printf("\n");
    printf("============================================\n");
    printf("              ADDRESS BOOK\n");
    printf("============================================\n");
    printf("  Contacts loaded: %d\n", addressBook.contactCount);

    /* Main menu loop — runs until the user chooses Exit */
    while (1)
    {
        printf("\n");
        printf("============================================\n");
        printf("                MAIN MENU\n");
        printf("============================================\n");
        printf("  1. Add Contact\n");
        printf("  2. Search Contact\n");
        printf("  3. Edit Contact\n");
        printf("  4. Delete Contact\n");
        printf("  5. List Contacts\n");
        printf("  6. Exit\n");
        printf("============================================\n");
        printf("  Enter your choice (1-6): ");

        /* Validate: must be a number */
        if (scanf("%d", &choice) != 1)
        {
            printf("  Error: Invalid input. Please enter a number between 1 and 6.\n");
            clear_input_buffer();
            continue;
        }

        /* Validate: must be in range 1-6 */
        if (choice < 1 || choice > 6)
        {
            printf("  Error: Invalid choice. Please select a number between 1 and 6.\n");
            clear_input_buffer();
            continue;
        }

        /* Clear the '\n' left in buffer after scanf */
        clear_input_buffer();

        /* Dispatch to the selected operation */
        switch (choice)
        {
            case 1:
                add_contact(&addressBook);
                break;

            case 2:
                search_contact(&addressBook);
                break;

            case 3:
                edit_contact(&addressBook);
                break;

            case 4:
                delete_contact(&addressBook);
                break;

            case 5:
                list_contacts(&addressBook);
                break;

            case 6:
                /* Save before exiting */
                push_data_to_file(&addressBook, FILE_NAME);
                printf("\n  Data saved successfully.\n");
                printf("  Thank you for using Address Book!\n\n");
                return 0;
        }
    }

    return 0;
}
