#include "person.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

Person* person_create(const char *id, const char *name, const char *gender, 
                      int birth_year, int death_year, int is_living, const char *bio) {
    if (!id || strlen(id) == 0 || !name || strlen(name) == 0) {
        return NULL;
    }

    Person *p = (Person*)malloc(sizeof(Person));
    if (!p) return NULL;

    memset(p, 0, sizeof(Person));
    strncpy(p->id, id, MAX_ID_LEN - 1);
    strncpy(p->name, name, MAX_NAME_LEN - 1);
    strncpy(p->gender, (gender && strlen(gender) > 0) ? gender : "Unknown", MAX_GENDER_LEN - 1);
    p->birth_year = birth_year;
    p->death_year = death_year;
    p->is_living = is_living;
    if (bio) {
        strncpy(p->bio, bio, MAX_BIO_LEN - 1);
    }
    p->generation = 0;

    return p;
}

void person_free(Person *p) {
    if (p) {
        free(p);
    }
}

Person* person_clone(const Person *p) {
    if (!p) return NULL;
    Person *c = (Person*)malloc(sizeof(Person));
    if (!c) return NULL;
    memcpy(c, p, sizeof(Person));
    return c;
}

bool person_validate(const Person *p, char *error_msg, size_t error_msg_len) {
    if (!p) {
        if (error_msg) snprintf(error_msg, error_msg_len, "Person pointer is null.");
        return false;
    }
    if (strlen(p->id) == 0) {
        if (error_msg) snprintf(error_msg, error_msg_len, "Person ID cannot be empty.");
        return false;
    }
    // Check ID contains only alphanumeric, dash, or underscore
    for (size_t i = 0; i < strlen(p->id); i++) {
        char ch = p->id[i];
        if (!isalnum(ch) && ch != '_' && ch != '-') {
            if (error_msg) snprintf(error_msg, error_msg_len, "Person ID can only contain alphanumeric characters, hyphens, and underscores.");
            return false;
        }
    }
    if (strlen(p->name) == 0) {
        if (error_msg) snprintf(error_msg, error_msg_len, "Person Name cannot be empty.");
        return false;
    }
    if (p->birth_year > 0 && p->death_year > 0 && p->death_year < p->birth_year) {
        if (error_msg) snprintf(error_msg, error_msg_len, "Death year (%d) cannot be before birth year (%d).", p->death_year, p->birth_year);
        return false;
    }
    return true;
}

cJSON* person_to_json(const Person *p) {
    if (!p) return NULL;
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "id", p->id);
    cJSON_AddStringToObject(obj, "name", p->name);
    cJSON_AddStringToObject(obj, "gender", p->gender);
    cJSON_AddNumberToObject(obj, "birth_year", p->birth_year);
    cJSON_AddNumberToObject(obj, "death_year", p->death_year);
    cJSON_AddBoolToObject(obj, "is_living", p->is_living);
    cJSON_AddStringToObject(obj, "bio", p->bio);
    cJSON_AddNumberToObject(obj, "generation", p->generation);
    return obj;
}

Person* person_from_json(const cJSON *json) {
    if (!json || !cJSON_IsObject(json)) return NULL;

    const cJSON *j_id = cJSON_GetObjectItemCaseSensitive(json, "id");
    const cJSON *j_name = cJSON_GetObjectItemCaseSensitive(json, "name");
    const cJSON *j_gender = cJSON_GetObjectItemCaseSensitive(json, "gender");
    const cJSON *j_birth = cJSON_GetObjectItemCaseSensitive(json, "birth_year");
    const cJSON *j_death = cJSON_GetObjectItemCaseSensitive(json, "death_year");
    const cJSON *j_living = cJSON_GetObjectItemCaseSensitive(json, "is_living");
    const cJSON *j_bio = cJSON_GetObjectItemCaseSensitive(json, "bio");

    if (!cJSON_IsString(j_id) || (j_id->valuestring == NULL)) return NULL;
    if (!cJSON_IsString(j_name) || (j_name->valuestring == NULL)) return NULL;

    const char *id = j_id->valuestring;
    const char *name = j_name->valuestring;
    const char *gender = (cJSON_IsString(j_gender) && j_gender->valuestring) ? j_gender->valuestring : "Unknown";
    int birth_year = (cJSON_IsNumber(j_birth)) ? j_birth->valueint : 0;
    int death_year = (cJSON_IsNumber(j_death)) ? j_death->valueint : 0;
    int is_living = 1;
    if (cJSON_IsBool(j_living)) {
        is_living = cJSON_IsTrue(j_living) ? 1 : 0;
    } else if (cJSON_IsNumber(j_living)) {
        is_living = j_living->valueint ? 1 : 0;
    }
    const char *bio = (cJSON_IsString(j_bio) && j_bio->valuestring) ? j_bio->valuestring : "";

    return person_create(id, name, gender, birth_year, death_year, is_living, bio);
}
