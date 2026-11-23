#ifndef GRAPH_H
#define GRAPH_H

#include <stdbool.h>
#include <stddef.h>
#include "person.h"
#include "hashmap.h"
#include "cJSON.h"

/**
 * Adjacency list edge representing a relationship link.
 */
typedef struct Edge {
    char target_id[MAX_ID_LEN];
    struct GraphVertex *target;   // Direct pointer for rapid traversal
    struct Edge *next;            // Linked list pointer
} Edge;

/**
 * Graph Vertex representing a unique Person and their relationship adjacency lists.
 */
typedef struct GraphVertex {
    Person *person;               // Unique person stored once
    Edge *children;               // Adjacency linked list of child edges (Parent -> Child)
    Edge *parents;                // Adjacency linked list of parent edges (Child -> Parent)
    Edge *spouses;                // Separate list of spouse edges
    int in_degree;                // Number of parents
    int out_degree;               // Number of children
    struct GraphVertex *next;     // Linked list of all vertices in graph
} GraphVertex;

/**
 * FamilyGraph Structure.
 * Maintains vertex list and $O(1)$ Hash Table index.
 */
typedef struct FamilyGraph {
    GraphVertex *vertices_head;   // Head of all vertices
    HashMap *vertex_map;          // Hash map: person_id -> GraphVertex*
    size_t vertex_count;          // Number of people
    size_t edge_count_parent_child;// Number of directed parent-child edges
    size_t edge_count_spouse;     // Number of mutual spouse pairs
} FamilyGraph;

/**
 * Creates a new empty FamilyGraph.
 */
FamilyGraph* graph_create(void);

/**
 * Frees the entire graph, vertices, edges, and people.
 */
void graph_destroy(FamilyGraph *graph);

/**
 * Adds a new person as a vertex in the graph.
 * Returns true on success, false if ID already exists or allocation fails.
 */
bool graph_add_person(FamilyGraph *graph, Person *person, char *err_msg, size_t err_len);

/**
 * Gets a graph vertex by person ID (O(1) average via HashMap).
 */
GraphVertex* graph_get_vertex(const FamilyGraph *graph, const char *person_id);

/**
 * Gets a person pointer by person ID.
 */
Person* graph_get_person(const FamilyGraph *graph, const char *person_id);

/**
 * Updates an existing person's details.
 */
bool graph_update_person(FamilyGraph *graph, const Person *updated, char *err_msg, size_t err_len);

/**
 * Deletes a person and cleans up all incoming/outgoing parent-child edges and spouse links.
 */
bool graph_remove_person(FamilyGraph *graph, const char *person_id, char *err_msg, size_t err_len);

/**
 * Adds a directed parent-child relationship (parent_id -> child_id).
 * Validates existence, duplicates, and checks for ancestry cycles.
 */
bool graph_add_parent_child(FamilyGraph *graph, const char *parent_id, const char *child_id, 
                           char *err_msg, size_t err_len);

/**
 * Removes a directed parent-child relationship.
 */
bool graph_remove_parent_child(FamilyGraph *graph, const char *parent_id, const char *child_id, 
                              char *err_msg, size_t err_len);

/**
 * Adds a mutual spouse relationship.
 */
bool graph_add_spouse(FamilyGraph *graph, const char *person_a, const char *person_b, 
                     char *err_msg, size_t err_len);

/**
 * Removes a mutual spouse relationship.
 */
bool graph_remove_spouse(FamilyGraph *graph, const char *person_a, const char *person_b, 
                        char *err_msg, size_t err_len);

/**
 * Serializes the entire graph structure into cJSON.
 */
cJSON* graph_to_json(const FamilyGraph *graph);

/**
 * Checks if a specific directed edge exists.
 */
bool graph_has_child(const GraphVertex *parent_v, const char *child_id);
bool graph_has_parent(const GraphVertex *child_v, const char *parent_id);
bool graph_has_spouse(const GraphVertex *v, const char *spouse_id);

#endif /* GRAPH_H */
