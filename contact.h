#ifndef CONTACT_H
#define CONTACT_H

/* =========================================================
   STANDARD LIBRARY INCLUDES
   ========================================================= */

#include <stdio.h>  /* printf, scanf, fgets, fopen, fclose, fprintf, fscanf, FILE */
#include <string.h> /* strlen, strcpy, strcmp, strstr, strcspn, strchr, strrchr   */

/* =========================================================
   CONSTANTS
   ========================================================= */

#define MAX_CONTACTS 100         /* Maximum number of contacts the address book can hold */
#define NAME_SIZE 50             /* Maximum characters for a name  (including '\0')       */
#define PHONE_SIZE 15            /* Maximum characters for a phone (including '\0')       */
#define EMAIL_SIZE 50            /* Maximum characters for an email (including '\0')      */
#define FILE_NAME "database.csv" /* CSV file used to save / load contacts      */

/* =========================================================
   STRUCTURES
   ========================================================= */

/*
   Contact
   -------
   Holds a single person's information.
   Each field is a fixed-size character array (C-string).
*/
struct Contact
{
    char name[NAME_SIZE];   /* Person's full name  */
    char phone[PHONE_SIZE]; /* 10-digit mobile number */
    char email[EMAIL_SIZE]; /* Email address */
};

/*
   AddressBook
   -----------
   The main data container.
   Holds all contacts and tracks search results.
*/
struct AddressBook
{
    struct Contact contacts[MAX_CONTACTS]; /* Array of all contacts */

    int contactCount; /* How many contacts are currently stored */

    int index_record[MAX_CONTACTS]; /* Positions (indices) of contacts that
                                       matched the most recent search          */

    int ir_size; /* Number of matches stored in index_record */
};

/* =========================================================
   FUNCTION PROTOTYPES
   ========================================================= */

/* --- File Handling --- */
void pull_file_data(struct AddressBook *addressBook, char *file_name);
void push_data_to_file(struct AddressBook *addressBook, char *file_name);

/* --- Contact Operations --- */
void add_contact(struct AddressBook *addressBook);
void search_contact(struct AddressBook *addressBook);
void edit_contact(struct AddressBook *addressBook);
void delete_contact(struct AddressBook *addressBook);
void list_contacts(struct AddressBook *addressBook);

/* --- Validation (used internally, also called standalone) --- */
void mobile_verify(char mobile[]);
void gmail_verify(struct AddressBook *addressBook, char gmail[]);

/* --- Search Operations (called by search_contact, edit_contact, delete_contact) --- */
void search_by_name(struct AddressBook *addressBook, char name[]);
void search_by_phone(struct AddressBook *addressBook, char phone[]);
void search_by_gmail(struct AddressBook *addressBook, char gmail[]);

/* --- Helper Functions (defined in helper.c) --- */
void remove_newline(char string[]);
void read_string(char string[], int size);
void clear_input_buffer(void);
int is_valid_mobile(char mobile[]);
int is_valid_email(char email[]);
int phone_already_exists(struct AddressBook *addressBook, char phone[], int ignored_index);
int email_already_exists(struct AddressBook *addressBook, char email[], int ignored_index);

#endif