#include "contact.h"

int main(void)
{
    struct AddressBook addressBook;

    int choice;

    /*
       Initialize Address Book
    */

    addressBook.contactCount = 0;
    addressBook.ir_size = 0;

    /*
       Load existing contacts
       from database.csv
    */

    pull_file_data(
        &addressBook,
        FILE_NAME);

    printf("\n");
    printf("============================================\n");
    printf("             ADDRESS BOOK\n");
    printf("============================================\n");

    printf(
        "Loaded contacts: %d\n",
        addressBook.contactCount);

    /*
       Main menu loop
    */

    while (1)
    {
        printf("\n");
        printf("============================================\n");
        printf("                 MAIN MENU\n");
        printf("============================================\n");

        printf("1. Add Contact\n");
        printf("2. Search Contact\n");
        printf("3. Edit Contact\n");
        printf("4. Delete Contact\n");
        printf("5. List Contacts\n");
        printf("6. Exit\n");

        printf("============================================\n");

        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1)
        {
            printf(
                "\nInvalid input. "
                "Please enter a number.\n");

            /*
               Clear invalid input
            */

            int character;

            while (
                (character = getchar()) != '\n' &&
                character != EOF)
            {
                /* Clear buffer */
            }

            continue;
        }

        /*
           Clear newline after scanf
        */

        {
            int character;

            while (
                (character = getchar()) != '\n' &&
                character != EOF)
            {
                /* Clear buffer */
            }
        }

        /*
           Select operation
        */

        switch (choice)
        {
        case 1:

            add_contact(
                &addressBook);

            break;

        case 2:

            search_contact(
                &addressBook);

            break;

        case 3:

            edit_contact(
                &addressBook);

            break;

        case 4:

            delete_contact(
                &addressBook);

            break;

        case 5:

            list_contacts(
                &addressBook);

            break;

        case 6:

            /*
               Save before exiting
            */

            push_data_to_file(
                &addressBook,
                FILE_NAME);

            printf(
                "\nData saved successfully.\n");

            printf(
                "Thank you for using Address Book!\n");

            return 0;

        default:

            printf(
                "\nInvalid choice. "
                "Please select 1-6.\n");
        }
    }

    return 0;
}