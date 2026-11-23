#ifndef STORAGE_H
#define STORAGE_H

#include "graph.h"
#include <stdbool.h>

#define DEFAULT_PEOPLE_FILE "KinGraph/backend/data/people.dat"
#define DEFAULT_REL_FILE "KinGraph/backend/data/relationships.dat"

/**
 * Saves current family tree to persistent C data files.
 */
bool storage_save(const FamilyGraph *graph, const char *people_file, const char *rel_file);

/**
 * Loads family tree records from persistent C data files.
 */
bool storage_load(FamilyGraph *graph, const char *people_file, const char *rel_file, 
                  char *err_msg, size_t err_len);

/**
 * Populates the graph with rich fictional sample family data.
 */
void storage_load_sample_dataset(FamilyGraph *graph);

#endif /* STORAGE_H */
