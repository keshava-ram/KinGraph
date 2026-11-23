#include "graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Forward declaration of reachable check implemented below */
static bool is_reachable_parent_to_child(const FamilyGraph *graph, const char *src_id, const char *dest_id);

static Edge* edge_create(const char *target_id, GraphVertex *target) {
    Edge *e = (Edge*)malloc(sizeof(Edge));
    if (!e) return NULL;
    strncpy(e->target_id, target_id, MAX_ID_LEN - 1);
    e->target_id[MAX_ID_LEN - 1] = '\0';
    e->target = target;
    e->next = NULL;
    return e;
}

static void edge_list_free(Edge *head) {
    Edge *curr = head;
    while (curr) {
        Edge *next = curr->next;
        free(curr);
        curr = next;
    }
}

static bool edge_list_remove(Edge **head_ref, const char *target_id) {
    if (!head_ref || !*head_ref) return false;

    Edge *curr = *head_ref;
    Edge *prev = NULL;

    while (curr) {
        if (strcmp(curr->target_id, target_id) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                *head_ref = curr->next;
            }
            free(curr);
            return true;
        }
        prev = curr;
        curr = curr->next;
    }
    return false;
}

static bool edge_list_contains(const Edge *head, const char *target_id) {
    const Edge *curr = head;
    while (curr) {
        if (strcmp(curr->target_id, target_id) == 0) {
            return true;
        }
        curr = curr->next;
    }
    return false;
}

FamilyGraph* graph_create(void) {
    FamilyGraph *g = (FamilyGraph*)malloc(sizeof(FamilyGraph));
    if (!g) return NULL;

    g->vertices_head = NULL;
    g->vertex_map = hashmap_create(127);
    g->vertex_count = 0;
    g->edge_count_parent_child = 0;
    g->edge_count_spouse = 0;
    return g;
}

void graph_destroy(FamilyGraph *graph) {
    if (!graph) return;

    GraphVertex *curr = graph->vertices_head;
    while (curr) {
        GraphVertex *next = curr->next;
        if (curr->person) {
            person_free(curr->person);
        }
        edge_list_free(curr->children);
        edge_list_free(curr->parents);
        edge_list_free(curr->spouses);
        free(curr);
        curr = next;
    }

    if (graph->vertex_map) {
        hashmap_destroy(graph->vertex_map, false);
    }
    free(graph);
}

bool graph_add_person(FamilyGraph *graph, Person *person, char *err_msg, size_t err_len) {
    if (!graph || !person) {
        if (err_msg) snprintf(err_msg, err_len, "Invalid parameters.");
        return false;
    }

    // Check duplicate ID
    if (hashmap_contains(graph->vertex_map, person->id)) {
        if (err_msg) snprintf(err_msg, err_len, "Person with ID '%s' already exists in the graph.", person->id);
        return false;
    }

    GraphVertex *v = (GraphVertex*)malloc(sizeof(GraphVertex));
    if (!v) {
        if (err_msg) snprintf(err_msg, err_len, "Memory allocation failure for vertex.");
        return false;
    }

    v->person = person;
    v->children = NULL;
    v->parents = NULL;
    v->spouses = NULL;
    v->in_degree = 0;
    v->out_degree = 0;

    // Add to vertex list
    v->next = graph->vertices_head;
    graph->vertices_head = v;
    graph->vertex_count++;

    // Add to hash table
    hashmap_put(graph->vertex_map, person->id, v);

    return true;
}

GraphVertex* graph_get_vertex(const FamilyGraph *graph, const char *person_id) {
    if (!graph || !person_id) return NULL;
    return (GraphVertex*)hashmap_get(graph->vertex_map, person_id);
}

Person* graph_get_person(const FamilyGraph *graph, const char *person_id) {
    GraphVertex *v = graph_get_vertex(graph, person_id);
    return v ? v->person : NULL;
}

bool graph_update_person(FamilyGraph *graph, const Person *updated, char *err_msg, size_t err_len) {
    if (!graph || !updated) {
        if (err_msg) snprintf(err_msg, err_len, "Invalid parameters.");
        return false;
    }

    GraphVertex *v = graph_get_vertex(graph, updated->id);
    if (!v || !v->person) {
        if (err_msg) snprintf(err_msg, err_len, "Person with ID '%s' not found.", updated->id);
        return false;
    }

    // Update mutable fields (ID remains immutable key)
    strncpy(v->person->name, updated->name, MAX_NAME_LEN - 1);
    strncpy(v->person->gender, updated->gender, MAX_GENDER_LEN - 1);
    v->person->birth_year = updated->birth_year;
    v->person->death_year = updated->death_year;
    v->person->is_living = updated->is_living;
    strncpy(v->person->bio, updated->bio, MAX_BIO_LEN - 1);

    return true;
}

bool graph_remove_person(FamilyGraph *graph, const char *person_id, char *err_msg, size_t err_len) {
    if (!graph || !person_id) {
        if (err_msg) snprintf(err_msg, err_len, "Invalid parameters.");
        return false;
    }

    GraphVertex *target_v = graph_get_vertex(graph, person_id);
    if (!target_v) {
        if (err_msg) snprintf(err_msg, err_len, "Person with ID '%s' not found in graph.", person_id);
        return false;
    }

    // 1. Remove references to this person from all their parents' children lists
    Edge *p_edge = target_v->parents;
    while (p_edge) {
        GraphVertex *parent_v = p_edge->target;
        if (parent_v) {
            if (edge_list_remove(&parent_v->children, person_id)) {
                parent_v->out_degree--;
                if (graph->edge_count_parent_child > 0) graph->edge_count_parent_child--;
            }
        }
        p_edge = p_edge->next;
    }

    // 2. Remove references to this person from all their children's parents lists
    Edge *c_edge = target_v->children;
    while (c_edge) {
        GraphVertex *child_v = c_edge->target;
        if (child_v) {
            if (edge_list_remove(&child_v->parents, person_id)) {
                child_v->in_degree--;
                if (graph->edge_count_parent_child > 0) graph->edge_count_parent_child--;
            }
        }
        c_edge = c_edge->next;
    }

    // 3. Remove spouse connections from mutual spouses
    Edge *sp_edge = target_v->spouses;
    while (sp_edge) {
        GraphVertex *sp_v = sp_edge->target;
        if (sp_v) {
            if (edge_list_remove(&sp_v->spouses, person_id)) {
                if (graph->edge_count_spouse > 0) graph->edge_count_spouse--;
            }
        }
        sp_edge = sp_edge->next;
    }

    // 4. Remove from vertices_head linked list
    GraphVertex *curr = graph->vertices_head;
    GraphVertex *prev = NULL;
    while (curr) {
        if (curr == target_v) {
            if (prev) {
                prev->next = curr->next;
            } else {
                graph->vertices_head = curr->next;
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }

    // 5. Remove from hash table
    hashmap_remove(graph->vertex_map, person_id);

    // 6. Free memory for the vertex and edges
    edge_list_free(target_v->children);
    edge_list_free(target_v->parents);
    edge_list_free(target_v->spouses);
    if (target_v->person) {
        person_free(target_v->person);
    }
    free(target_v);

    graph->vertex_count--;
    return true;
}

/**
 * Checks reachability along directed parent-to-child edges using an iterative queue (BFS).
 * Returns true if dest_id is reachable from src_id (i.e. dest_id is a descendant of src_id).
 */
static bool is_reachable_parent_to_child(const FamilyGraph *graph, const char *src_id, const char *dest_id) {
    if (!graph || !src_id || !dest_id) return false;
    if (strcmp(src_id, dest_id) == 0) return true;

    // Use a visited hash set or table for traversal
    HashMap *visited = hashmap_create(64);
    
    // Allocate a small traversal queue for IDs
    const char *queue[1024];
    int front = 0, rear = 0;

    queue[rear++] = src_id;
    hashmap_put(visited, src_id, (void*)1);

    bool reachable = false;

    while (front < rear) {
        const char *curr_id = queue[front++];
        if (strcmp(curr_id, dest_id) == 0) {
            reachable = true;
            break;
        }

        GraphVertex *v = graph_get_vertex(graph, curr_id);
        if (v) {
            Edge *e = v->children;
            while (e) {
                if (!hashmap_contains(visited, e->target_id)) {
                    hashmap_put(visited, e->target_id, (void*)1);
                    if (rear < 1024) {
                        queue[rear++] = e->target_id;
                    }
                }
                e = e->next;
            }
        }
    }

    hashmap_destroy(visited, false);
    return reachable;
}

bool graph_add_parent_child(FamilyGraph *graph, const char *parent_id, const char *child_id, 
                           char *err_msg, size_t err_len) {
    if (!graph || !parent_id || !child_id) {
        if (err_msg) snprintf(err_msg, err_len, "Invalid parameters.");
        return false;
    }

    if (strcmp(parent_id, child_id) == 0) {
        if (err_msg) snprintf(err_msg, err_len, "An individual cannot be their own parent.");
        return false;
    }

    GraphVertex *p_v = graph_get_vertex(graph, parent_id);
    if (!p_v) {
        if (err_msg) snprintf(err_msg, err_len, "Parent ID '%s' does not exist in graph.", parent_id);
        return false;
    }

    GraphVertex *c_v = graph_get_vertex(graph, child_id);
    if (!c_v) {
        if (err_msg) snprintf(err_msg, err_len, "Child ID '%s' does not exist in graph.", child_id);
        return false;
    }

    // Check if relationship already exists
    if (edge_list_contains(p_v->children, child_id)) {
        if (err_msg) snprintf(err_msg, err_len, "Relationship already exists between '%s' and '%s'.", parent_id, child_id);
        return false;
    }

    // Cycle detection: Can parent_id be reached from child_id?
    // If child_id is already an ancestor of parent_id, adding parent -> child creates a cycle!
    if (is_reachable_parent_to_child(graph, child_id, parent_id)) {
        if (err_msg) snprintf(err_msg, err_len, "Cycle detected: '%s' is already a descendant of '%s'. Adding this edge would create an ancestral loop.", parent_id, child_id);
        return false;
    }

    // Optional biological check: maximum 2 parents
    if (c_v->in_degree >= 2) {
        if (err_msg) snprintf(err_msg, err_len, "Child '%s' already has 2 parents recorded.", child_id);
        return false;
    }

    // Add to parent's children list
    Edge *out_edge = edge_create(child_id, c_v);
    if (!out_edge) {
        if (err_msg) snprintf(err_msg, err_len, "Memory allocation failed for edge.");
        return false;
    }
    out_edge->next = p_v->children;
    p_v->children = out_edge;
    p_v->out_degree++;

    // Add to child's parents list
    Edge *in_edge = edge_create(parent_id, p_v);
    if (!in_edge) {
        edge_list_remove(&p_v->children, child_id);
        p_v->out_degree--;
        if (err_msg) snprintf(err_msg, err_len, "Memory allocation failed for reverse edge.");
        return false;
    }
    in_edge->next = c_v->parents;
    c_v->parents = in_edge;
    c_v->in_degree++;

    graph->edge_count_parent_child++;
    return true;
}

bool graph_remove_parent_child(FamilyGraph *graph, const char *parent_id, const char *child_id, 
                              char *err_msg, size_t err_len) {
    if (!graph || !parent_id || !child_id) {
        if (err_msg) snprintf(err_msg, err_len, "Invalid parameters.");
        return false;
    }

    GraphVertex *p_v = graph_get_vertex(graph, parent_id);
    GraphVertex *c_v = graph_get_vertex(graph, child_id);

    if (!p_v || !c_v) {
        if (err_msg) snprintf(err_msg, err_len, "Specified member not found.");
        return false;
    }

    bool removed_child = edge_list_remove(&p_v->children, child_id);
    bool removed_parent = edge_list_remove(&c_v->parents, parent_id);

    if (removed_child) p_v->out_degree--;
    if (removed_parent) c_v->in_degree--;

    if (removed_child || removed_parent) {
        if (graph->edge_count_parent_child > 0) graph->edge_count_parent_child--;
        return true;
    }

    if (err_msg) snprintf(err_msg, err_len, "No parent-child relationship between '%s' and '%s'.", parent_id, child_id);
    return false;
}

bool graph_add_spouse(FamilyGraph *graph, const char *person_a, const char *person_b, 
                     char *err_msg, size_t err_len) {
    if (!graph || !person_a || !person_b) {
        if (err_msg) snprintf(err_msg, err_len, "Invalid parameters.");
        return false;
    }

    if (strcmp(person_a, person_b) == 0) {
        if (err_msg) snprintf(err_msg, err_len, "An individual cannot be their own spouse.");
        return false;
    }

    GraphVertex *va = graph_get_vertex(graph, person_a);
    GraphVertex *vb = graph_get_vertex(graph, person_b);

    if (!va || !vb) {
        if (err_msg) snprintf(err_msg, err_len, "One or both individuals not found.");
        return false;
    }

    if (edge_list_contains(va->spouses, person_b)) {
        if (err_msg) snprintf(err_msg, err_len, "Spouse relationship already exists.");
        return false;
    }

    Edge *ea = edge_create(person_b, vb);
    Edge *eb = edge_create(person_a, va);
    if (!ea || !eb) {
        if (ea) free(ea);
        if (eb) free(eb);
        if (err_msg) snprintf(err_msg, err_len, "Memory allocation failed for spouse edge.");
        return false;
    }

    ea->next = va->spouses;
    va->spouses = ea;

    eb->next = vb->spouses;
    vb->spouses = eb;

    graph->edge_count_spouse++;
    return true;
}

bool graph_remove_spouse(FamilyGraph *graph, const char *person_a, const char *person_b, 
                        char *err_msg, size_t err_len) {
    if (!graph || !person_a || !person_b) {
        if (err_msg) snprintf(err_msg, err_len, "Invalid parameters.");
        return false;
    }

    GraphVertex *va = graph_get_vertex(graph, person_a);
    GraphVertex *vb = graph_get_vertex(graph, person_b);

    if (!va || !vb) {
        if (err_msg) snprintf(err_msg, err_len, "Individuals not found.");
        return false;
    }

    bool r1 = edge_list_remove(&va->spouses, person_b);
    bool r2 = edge_list_remove(&vb->spouses, person_a);

    if (r1 || r2) {
        if (graph->edge_count_spouse > 0) graph->edge_count_spouse--;
        return true;
    }

    if (err_msg) snprintf(err_msg, err_len, "No spouse relationship found between '%s' and '%s'.", person_a, person_b);
    return false;
}

bool graph_has_child(const GraphVertex *parent_v, const char *child_id) {
    return parent_v ? edge_list_contains(parent_v->children, child_id) : false;
}

bool graph_has_parent(const GraphVertex *child_v, const char *parent_id) {
    return child_v ? edge_list_contains(child_v->parents, parent_id) : false;
}

bool graph_has_spouse(const GraphVertex *v, const char *spouse_id) {
    return v ? edge_list_contains(v->spouses, spouse_id) : false;
}

cJSON* graph_to_json(const FamilyGraph *graph) {
    if (!graph) return NULL;

    cJSON *root = cJSON_CreateObject();
    cJSON *people_arr = cJSON_CreateArray();
    cJSON *pc_edges_arr = cJSON_CreateArray();
    cJSON *sp_edges_arr = cJSON_CreateArray();

    // Iterate through all vertices
    GraphVertex *v = graph->vertices_head;
    while (v) {
        cJSON_AddItemToArray(people_arr, person_to_json(v->person));

        // Parent -> Child edges
        Edge *c = v->children;
        while (c) {
            cJSON *edge_obj = cJSON_CreateObject();
            cJSON_AddStringToObject(edge_obj, "parent_id", v->person->id);
            cJSON_AddStringToObject(edge_obj, "child_id", c->target_id);
            cJSON_AddItemToArray(pc_edges_arr, edge_obj);
            c = c->next;
        }

        // Spouse edges (only add once per pair: id1 < id2 to avoid duplicates)
        Edge *s = v->spouses;
        while (s) {
            if (strcmp(v->person->id, s->target_id) < 0) {
                cJSON *sp_obj = cJSON_CreateObject();
                cJSON_AddStringToObject(sp_obj, "person_a", v->person->id);
                cJSON_AddStringToObject(sp_obj, "person_b", s->target_id);
                cJSON_AddItemToArray(sp_edges_arr, sp_obj);
            }
            s = s->next;
        }

        v = v->next;
    }

    cJSON_AddItemToObject(root, "people", people_arr);
    cJSON_AddItemToObject(root, "parent_child_edges", pc_edges_arr);
    cJSON_AddItemToObject(root, "spouse_edges", sp_edges_arr);
    cJSON_AddNumberToObject(root, "total_people", graph->vertex_count);
    cJSON_AddNumberToObject(root, "total_parent_child", graph->edge_count_parent_child);
    cJSON_AddNumberToObject(root, "total_spouses", graph->edge_count_spouse);

    return root;
}
