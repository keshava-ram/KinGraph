#ifndef PERSON_H
#define PERSON_H

#include <stdbool.h>
#include "cJSON.h"

#define MAX_ID_LEN 64
#define MAX_NAME_LEN 128
#define MAX_GENDER_LEN 16
#define MAX_BIO_LEN 256

/**
 * Person Structure
 * Represents an individual in the KinGraph family tree.
 */
typedef struct Person {
    char id[MAX_ID_LEN];
    char name[MAX_NAME_LEN];
    char gender[MAX_GENDER_LEN]; // "Male", "Female", "Other", "Unknown"
    int birth_year;              // e.g. 1955 (0 if unknown)
    int death_year;              // e.g. 2020 (0 if living or unknown)
    int is_living;               // 1 = living, 0 = deceased
    char bio[MAX_BIO_LEN];       // brief notes, profession, etc.
    int generation;              // dynamically calculated level (1 = root/oldest)
} Person;

/**
 * Creates a dynamically allocated Person.
 */
Person* person_create(const char *id, const char *name, const char *gender, 
                      int birth_year, int death_year, int is_living, const char *bio);

/**
 * Frees a dynamically allocated Person.
 */
void person_free(Person *p);

/**
 * Deep-copies a Person.
 */
Person* person_clone(const Person *p);

/**
 * Validates basic person fields.
 * Returns true if valid, false otherwise (with errorMsg populated).
 */
bool person_validate(const Person *p, char *error_msg, size_t error_msg_len);

/**
 * Converts a Person to a cJSON object.
 */
cJSON* person_to_json(const Person *p);

/**
 * Parses a cJSON object into a newly allocated Person.
 */
Person* person_from_json(const cJSON *json);

#endif /* PERSON_H */
