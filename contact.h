#ifndef CONTACT_H
#define CONTACT_H

/* =========================================================
   STANDARD LIBRARY INCLUDES
   ========================================================= */

#include <stdio.h>    /* printf, scanf, fgets, fopen, fclose, fprintf, fscanf */
#include <string.h>   /* strlen, strcpy, strcmp, strstr, strcspn, strchr, strrchr */

/* =========================================================
   CONSTANTS
   ========================================================= */

#define MAX_CONTACTS  100          /* Maximum contacts the address book can hold */
#define NAME_SIZE     50           /* Max characters for a name  (+ '\0')        */
#define PHONE_SIZE    15           /* Max characters for a phone (+ '\0')        */
#define EMAIL_SIZE    100          /* Max characters for an email (+ '\0')       */
#define ERROR_SIZE    100          /* Max characters for a validation error msg  */
#define FILE_NAME     "database.csv"  /* CSV file used to save and load contacts */

/* =========================================================
   STRUCTURES
   ========================================================= */

/* Contact — holds one person's name, phone, and email */
struct Contact
{
    char name[NAME_SIZE];
    char phone[PHONE_SIZE];
    char email[EMAIL_SIZE];
};

/* AddressBook — main container for all contacts and search results */
struct AddressBook
{
    struct Contact contacts[MAX_CONTACTS]; /* Array of all stored contacts        */
    int contactCount;                      /* Number of contacts currently stored */
    int index_record[MAX_CONTACTS];        /* Indices of contacts matching search */
    int ir_size;                           /* Number of matches in index_record   */
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

/* --- Validation wrappers (print messages) --- */
void mobile_verify(char mobile[]);
void gmail_verify(struct AddressBook *addressBook, char gmail[]);

/* --- Search Operations --- */
void search_by_name(struct AddressBook *addressBook, char name[]);
void search_by_phone(struct AddressBook *addressBook, char phone[]);
void search_by_gmail(struct AddressBook *addressBook, char gmail[]);

/* --- Core Validators (return 1=valid, 0=invalid; fill error_msg on failure) --- */
int validate_name(char name[], char error_msg[]);
int validate_mobile(char mobile[], char error_msg[]);
int validate_email(char email[], char error_msg[]);

/* --- Legacy single-return validators (used internally) --- */
int is_valid_mobile(char mobile[]);
int is_valid_email(char email[]);

/* --- Helper Utilities --- */
void remove_newline(char string[]);
void read_string(char string[], int size);
void clear_input_buffer(void);
int  phone_already_exists(struct AddressBook *addressBook, char phone[], int ignored_index);
int  email_already_exists(struct AddressBook *addressBook, char email[], int ignored_index);

#endif
