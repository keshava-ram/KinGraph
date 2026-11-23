#ifndef ALGORITHMS_H
#define ALGORITHMS_H

#include "graph.h"
#include "cJSON.h"
#include <stdbool.h>

/**
 * Computes family generations using Breadth-First Search (BFS) with custom Queue.
 * Assigns generation levels (1, 2, 3...) to each person.
 */
void algo_compute_generations(FamilyGraph *graph);

/**
 * Returns JSON array of all generations and the members belonging to each.
 */
cJSON* algo_get_generations_json(FamilyGraph *graph);

/**
 * Retrieves all ancestors of a person using iterative Depth-First Search (DFS) with custom Stack.
 * Returns cJSON object containing list of ancestors, distances, and DSA execution trace.
 */
cJSON* algo_get_ancestors(const FamilyGraph *graph, const char *person_id);

/**
 * Retrieves all descendants of a person level-by-level using Breadth-First Search (BFS) with custom Queue.
 * Returns cJSON object containing list of descendants, distances, and DSA execution trace.
 */
cJSON* algo_get_descendants(const FamilyGraph *graph, const char *person_id);

/**
 * Finds all common ancestors between two people, highlighting the Lowest Common Ancestor (LCA).
 * Returns cJSON array of common ancestors.
 */
cJSON* algo_find_common_ancestors(const FamilyGraph *graph, const char *id_a, const char *id_b);

/**
 * Finds the shortest unweighted relationship path between two people using BFS with custom Queue.
 * Returns cJSON object with step-by-step path transitions and total distance.
 */
cJSON* algo_find_relationship_path(const FamilyGraph *graph, const char *id_a, const char *id_b);

/**
 * Classifies the exact biological or legal relationship between two individuals.
 * Returns cJSON object with relationship name, common ancestors, kinship degree, and path.
 */
cJSON* algo_classify_relationship(const FamilyGraph *graph, const char *id_a, const char *id_b);

/**
 * Validates whether adding a directed parent-child edge (parent_id -> child_id)
 * would create a cycle (ancestry paradox) using iterative DFS with custom Stack.
 */
bool algo_detect_cycle(const FamilyGraph *graph, const char *parent_id, const char *child_id);

#endif /* ALGORITHMS_H */
