#include "storage.h"
#include "algorithms.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void trim_newline(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

#include <sys/stat.h>

static void resolve_path(const char *override_path, const char *filename, char *out_buf, size_t out_len) {
    if (override_path && strlen(override_path) > 0) {
        strncpy(out_buf, override_path, out_len - 1);
        out_buf[out_len - 1] = '\0';
        return;
    }

    struct stat st;
    if (stat("backend/data", &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(out_buf, out_len, "backend/data/%s", filename);
    } else {
        snprintf(out_buf, out_len, "KinGraph/backend/data/%s", filename);
    }
}

bool storage_save(const FamilyGraph *graph, const char *people_file, const char *rel_file) {
    if (!graph) return false;

    char p_path[512];
    char r_path[512];
    resolve_path(people_file, "people.dat", p_path, sizeof(p_path));
    resolve_path(rel_file, "relationships.dat", r_path, sizeof(r_path));

    FILE *fp_p = fopen(p_path, "w");
    if (!fp_p) {
        perror("Failed to open people file for saving");
        return false;
    }

    FILE *fp_r = fopen(r_path, "w");
    if (!fp_r) {
        perror("Failed to open relationships file for saving");
        fclose(fp_p);
        return false;
    }

    // Save people: ID|Name|Gender|BirthYear|DeathYear|IsLiving|Bio
    GraphVertex *v = graph->vertices_head;
    while (v) {
        if (v->person) {
            fprintf(fp_p, "%s|%s|%s|%d|%d|%d|%s\n",
                    v->person->id,
                    v->person->name,
                    v->person->gender,
                    v->person->birth_year,
                    v->person->death_year,
                    v->person->is_living,
                    v->person->bio);
        }
        v = v->next;
    }
    fclose(fp_p);

    // Save relationships
    v = graph->vertices_head;
    while (v) {
        // Parent-child edges
        Edge *c = v->children;
        while (c) {
            fprintf(fp_r, "PARENT_CHILD|%s|%s\n", v->person->id, c->target_id);
            c = c->next;
        }

        // Spouse edges (only once: a < b)
        Edge *s = v->spouses;
        while (s) {
            if (strcmp(v->person->id, s->target_id) < 0) {
                fprintf(fp_r, "SPOUSE|%s|%s\n", v->person->id, s->target_id);
            }
            s = s->next;
        }

        v = v->next;
    }
    fclose(fp_r);

    return true;
}

bool storage_load(FamilyGraph *graph, const char *people_file, const char *rel_file, 
                  char *err_msg, size_t err_len) {
    if (!graph) return false;

    char p_path[512];
    char r_path[512];
    resolve_path(people_file, "people.dat", p_path, sizeof(p_path));
    resolve_path(rel_file, "relationships.dat", r_path, sizeof(r_path));

    FILE *fp_p = fopen(p_path, "r");
    if (!fp_p) {
        if (err_msg) snprintf(err_msg, err_len, "Cannot open file '%s'.", p_path);
        return false;
    }

    FILE *fp_r = fopen(r_path, "r");
    if (!fp_r) {
        fclose(fp_p);
        if (err_msg) snprintf(err_msg, err_len, "Cannot open file '%s'.", r_path);
        return false;
    }

    char line[1024];

    // Read People
    while (fgets(line, sizeof(line), fp_p)) {
        trim_newline(line);
        if (strlen(line) == 0 || line[0] == '#') continue;

        char id[MAX_ID_LEN] = {0};
        char name[MAX_NAME_LEN] = {0};
        char gender[MAX_GENDER_LEN] = {0};
        int birth_year = 0;
        int death_year = 0;
        int is_living = 1;
        char bio[MAX_BIO_LEN] = {0};

        char *token = strtok(line, "|");
        if (token) strncpy(id, token, sizeof(id) - 1);

        token = strtok(NULL, "|");
        if (token) strncpy(name, token, sizeof(name) - 1);

        token = strtok(NULL, "|");
        if (token) strncpy(gender, token, sizeof(gender) - 1);

        token = strtok(NULL, "|");
        if (token) birth_year = atoi(token);

        token = strtok(NULL, "|");
        if (token) death_year = atoi(token);

        token = strtok(NULL, "|");
        if (token) is_living = atoi(token);

        token = strtok(NULL, "|");
        if (token) strncpy(bio, token, sizeof(bio) - 1);

        if (strlen(id) > 0 && strlen(name) > 0) {
            Person *p = person_create(id, name, gender, birth_year, death_year, is_living, bio);
            if (p) {
                graph_add_person(graph, p, NULL, 0);
            }
        }
    }
    fclose(fp_p);

    // Read Relationships
    while (fgets(line, sizeof(line), fp_r)) {
        trim_newline(line);
        if (strlen(line) == 0 || line[0] == '#') continue;

        char type[32] = {0};
        char id_a[MAX_ID_LEN] = {0};
        char id_b[MAX_ID_LEN] = {0};

        char *token = strtok(line, "|");
        if (token) strncpy(type, token, sizeof(type) - 1);

        token = strtok(NULL, "|");
        if (token) strncpy(id_a, token, sizeof(id_a) - 1);

        token = strtok(NULL, "|");
        if (token) strncpy(id_b, token, sizeof(id_b) - 1);

        if (strcmp(type, "PARENT_CHILD") == 0) {
            graph_add_parent_child(graph, id_a, id_b, NULL, 0);
        } else if (strcmp(type, "SPOUSE") == 0) {
            graph_add_spouse(graph, id_a, id_b, NULL, 0);
        }
    }
    fclose(fp_r);

    algo_compute_generations(graph);
    return true;
}

void storage_load_sample_dataset(FamilyGraph *graph) {
    if (!graph) return;

    // Generation 1: Founders
    graph_add_person(graph, person_create("P01", "Arthur Sterling", "Male", 1940, 2018, 0, "Patriarch, civil engineer and town planner"), NULL, 0);
    graph_add_person(graph, person_create("P02", "Eleanor Vance", "Female", 1943, 0, 1, "Matriarch, landscape artist & botanical collector"), NULL, 0);
    graph_add_spouse(graph, "P01", "P02", NULL, 0);

    // Generation 2: Children and in-laws
    graph_add_person(graph, person_create("P03", "Julian Sterling", "Male", 1968, 0, 1, "Eldest son, professor of astrophysics"), NULL, 0);
    graph_add_person(graph, person_create("P04", "Clara Bennett", "Female", 1970, 0, 1, "Orthopedic surgeon, married to Julian"), NULL, 0);
    graph_add_spouse(graph, "P03", "P04", NULL, 0);
    graph_add_parent_child(graph, "P01", "P03", NULL, 0);
    graph_add_parent_child(graph, "P02", "P03", NULL, 0);

    graph_add_person(graph, person_create("P05", "Beatrice Sterling", "Female", 1972, 0, 1, "Daughter, oceanographer and conservationist"), NULL, 0);
    graph_add_person(graph, person_create("P06", "David Thorne", "Male", 1971, 0, 1, "Novelist and literary critic, married to Beatrice"), NULL, 0);
    graph_add_spouse(graph, "P05", "P06", NULL, 0);
    graph_add_parent_child(graph, "P01", "P05", NULL, 0);
    graph_add_parent_child(graph, "P02", "P05", NULL, 0);

    graph_add_person(graph, person_create("P07", "Cedric Sterling", "Male", 1978, 0, 1, "Youngest son, classical cellist & composer"), NULL, 0);
    graph_add_person(graph, person_create("P13", "Helena Croft", "Female", 1980, 0, 1, "Documentary photographer, married to Cedric"), NULL, 0);
    graph_add_spouse(graph, "P07", "P13", NULL, 0);
    graph_add_parent_child(graph, "P01", "P07", NULL, 0);
    graph_add_parent_child(graph, "P02", "P07", NULL, 0);

    // Generation 3: Grandchildren
    graph_add_person(graph, person_create("P08", "Lucas Sterling", "Male", 1995, 0, 1, "AI researcher & systems architect"), NULL, 0);
    graph_add_person(graph, person_create("P15", "Zoe Martinez", "Female", 1996, 0, 1, "Data scientist, married to Lucas"), NULL, 0);
    graph_add_spouse(graph, "P08", "P15", NULL, 0);
    graph_add_parent_child(graph, "P03", "P08", NULL, 0);
    graph_add_parent_child(graph, "P04", "P08", NULL, 0);

    graph_add_person(graph, person_create("P09", "Maya Sterling", "Female", 1998, 0, 1, "Renewable energy engineer, daughter of Julian & Clara"), NULL, 0);
    graph_add_parent_child(graph, "P03", "P09", NULL, 0);
    graph_add_parent_child(graph, "P04", "P09", NULL, 0);

    graph_add_person(graph, person_create("P10", "Sophia Thorne", "Female", 1997, 0, 1, "Concert pianist, daughter of Beatrice & David"), NULL, 0);
    graph_add_parent_child(graph, "P05", "P10", NULL, 0);
    graph_add_parent_child(graph, "P06", "P10", NULL, 0);

    graph_add_person(graph, person_create("P11", "Ethan Thorne", "Male", 2001, 0, 1, "Aerospace engineering student, son of Beatrice & David"), NULL, 0);
    graph_add_parent_child(graph, "P05", "P11", NULL, 0);
    graph_add_parent_child(graph, "P06", "P11", NULL, 0);

    graph_add_person(graph, person_create("P12", "Nora Sterling", "Female", 2008, 0, 1, "High school robotics champion, daughter of Cedric & Helena"), NULL, 0);
    graph_add_parent_child(graph, "P07", "P12", NULL, 0);
    graph_add_parent_child(graph, "P13", "P12", NULL, 0);

    // Generation 4: Great-Grandchildren
    graph_add_person(graph, person_create("P14", "Leo Sterling", "Male", 2023, 0, 1, "Great-grandson of Arthur & Eleanor, son of Lucas & Zoe"), NULL, 0);
    graph_add_parent_child(graph, "P08", "P14", NULL, 0);
    graph_add_parent_child(graph, "P15", "P14", NULL, 0);

    algo_compute_generations(graph);

    // Persist to files as well
    storage_save(graph, NULL, NULL);
}
