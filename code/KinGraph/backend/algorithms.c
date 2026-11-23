#include "algorithms.h"
#include "queue.h"
#include "stack.h"
#include "hashmap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper structure for BFS/DFS state tracking */
typedef struct PathNode {
    char id[MAX_ID_LEN];
    int distance;
    char edge_label[32]; // "parent_of", "child_of", "spouse_of"
    char prev_id[MAX_ID_LEN];
} PathNode;

static const char* get_gender_label(const char *gender, const char *male_str, const char *female_str, const char *neutral_str) {
    if (!gender) return neutral_str;
    if (strcasecmp(gender, "Male") == 0 || strcasecmp(gender, "M") == 0) return male_str;
    if (strcasecmp(gender, "Female") == 0 || strcasecmp(gender, "F") == 0) return female_str;
    return neutral_str;
}

void algo_compute_generations(FamilyGraph *graph) {
    if (!graph || graph->vertex_count == 0) return;

    // Reset all generations to 0
    GraphVertex *v = graph->vertices_head;
    while (v) {
        if (v->person) v->person->generation = 0;
        v = v->next;
    }

    Queue *q = queue_create();
    if (!q) return;

    // Phase 1: Identify root individuals (in_degree == 0, i.e., no recorded parents)
    v = graph->vertices_head;
    while (v) {
        if (v->in_degree == 0 && v->person) {
            v->person->generation = 1;
            queue_enqueue(q, v);

            // If root has a spouse with no parents, also set spouse to Gen 1
            Edge *sp = v->spouses;
            while (sp) {
                if (sp->target && sp->target->in_degree == 0 && sp->target->person && sp->target->person->generation == 0) {
                    sp->target->person->generation = 1;
                    queue_enqueue(q, sp->target);
                }
                sp = sp->next;
            }
        }
        v = v->next;
    }

    // Phase 2: BFS propagation of generations
    while (!queue_is_empty(q)) {
        GraphVertex *curr = (GraphVertex*)queue_dequeue(q);
        int current_gen = curr->person ? curr->person->generation : 1;

        // Traverse children: generation = current_gen + 1
        Edge *c = curr->children;
        while (c) {
            GraphVertex *child_v = c->target;
            if (child_v && child_v->person) {
                if (child_v->person->generation < current_gen + 1) {
                    child_v->person->generation = current_gen + 1;
                    queue_enqueue(q, child_v);
                }
            }
            c = c->next;
        }

        // Align spouses on the same generation level
        Edge *s = curr->spouses;
        while (s) {
            GraphVertex *spouse_v = s->target;
            if (spouse_v && spouse_v->person) {
                if (spouse_v->person->generation == 0) {
                    spouse_v->person->generation = current_gen;
                    queue_enqueue(q, spouse_v);
                }
            }
            s = s->next;
        }
    }

    // Phase 3: Any disconnected or isolated nodes get generation 1
    v = graph->vertices_head;
    while (v) {
        if (v->person && v->person->generation == 0) {
            v->person->generation = 1;
        }
        v = v->next;
    }

    queue_destroy(q, false);
}

cJSON* algo_get_generations_json(FamilyGraph *graph) {
    if (!graph) return NULL;

    algo_compute_generations(graph);

    // Find maximum generation
    int max_gen = 1;
    GraphVertex *v = graph->vertices_head;
    while (v) {
        if (v->person && v->person->generation > max_gen) {
            max_gen = v->person->generation;
        }
        v = v->next;
    }

    cJSON *result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "total_generations", max_gen);
    cJSON *gen_array = cJSON_CreateArray();

    for (int g = 1; g <= max_gen; g++) {
        cJSON *gen_obj = cJSON_CreateObject();
        cJSON_AddNumberToObject(gen_obj, "generation_number", g);

        char gen_name[64];
        if (g == 1) snprintf(gen_name, sizeof(gen_name), "1st Gen (Founders / Elders)");
        else if (g == 2) snprintf(gen_name, sizeof(gen_name), "2nd Gen (Parents / Children of Founders)");
        else if (g == 3) snprintf(gen_name, sizeof(gen_name), "3rd Gen (Grandchildren)");
        else if (g == 4) snprintf(gen_name, sizeof(gen_name), "4th Gen (Great-Grandchildren)");
        else snprintf(gen_name, sizeof(gen_name), "%dth Generation", g);
        cJSON_AddStringToObject(gen_obj, "generation_title", gen_name);

        cJSON *members = cJSON_CreateArray();
        v = graph->vertices_head;
        while (v) {
            if (v->person && v->person->generation == g) {
                cJSON_AddItemToArray(members, person_to_json(v->person));
            }
            v = v->next;
        }
        cJSON_AddItemToObject(gen_obj, "members", members);
        cJSON_AddItemToArray(gen_array, gen_obj);
    }

    cJSON_AddItemToObject(result, "generations", gen_array);
    return result;
}

cJSON* algo_get_ancestors(const FamilyGraph *graph, const char *person_id) {
    if (!graph || !person_id) return NULL;

    GraphVertex *target_v = graph_get_vertex(graph, person_id);
    if (!target_v || !target_v->person) return NULL;

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "target_id", person_id);
    cJSON_AddStringToObject(res, "target_name", target_v->person->name);

    cJSON *ancestors_arr = cJSON_CreateArray();
    cJSON *dsa_trace = cJSON_CreateArray();

    // Use custom Stack for iterative DFS traversal
    Stack *s = stack_create();
    HashMap *visited_dist = hashmap_create(64);

    typedef struct StackItem {
        GraphVertex *v;
        int distance;
    } StackItem;

    StackItem *init_item = (StackItem*)malloc(sizeof(StackItem));
    init_item->v = target_v;
    init_item->distance = 0;
    stack_push(s, init_item);

    char trace_buf[256];
    snprintf(trace_buf, sizeof(trace_buf), "DFS Init: Pushed target '%s' to custom Stack (depth 0)", target_v->person->name);
    cJSON_AddItemToArray(dsa_trace, cJSON_CreateString(trace_buf));

    while (!stack_is_empty(s)) {
        StackItem *item = (StackItem*)stack_pop(s);
        GraphVertex *curr = item->v;
        int dist = item->distance;
        free(item);

        if (dist > 0 && curr->person) {
            snprintf(trace_buf, sizeof(trace_buf), "DFS Pop: Explored ancestor '%s' at distance %d", curr->person->name, dist);
            cJSON_AddItemToArray(dsa_trace, cJSON_CreateString(trace_buf));
        }

        // Push parents of curr onto Stack
        Edge *p_edge = curr->parents;
        while (p_edge) {
            GraphVertex *parent_v = p_edge->target;
            if (parent_v && parent_v->person) {
                if (!hashmap_contains(visited_dist, parent_v->person->id)) {
                    int parent_dist = dist + 1;
                    hashmap_put(visited_dist, parent_v->person->id, (void*)(intptr_t)parent_dist);

                    // Create JSON ancestor record
                    cJSON *a_obj = cJSON_CreateObject();
                    cJSON_AddStringToObject(a_obj, "id", parent_v->person->id);
                    cJSON_AddStringToObject(a_obj, "name", parent_v->person->name);
                    cJSON_AddStringToObject(a_obj, "gender", parent_v->person->gender);
                    cJSON_AddNumberToObject(a_obj, "distance", parent_dist);

                    const char *label = "Ancestor";
                    if (parent_dist == 1) {
                        label = get_gender_label(parent_v->person->gender, "Father", "Mother", "Parent");
                    } else if (parent_dist == 2) {
                        label = get_gender_label(parent_v->person->gender, "Grandfather", "Grandmother", "Grandparent");
                    } else if (parent_dist == 3) {
                        label = get_gender_label(parent_v->person->gender, "Great-Grandfather", "Great-Grandmother", "Great-Grandparent");
                    } else if (parent_dist >= 4) {
                        label = "Great-Great-Grandparent";
                    }
                    cJSON_AddStringToObject(a_obj, "relationship_label", label);
                    cJSON_AddItemToArray(ancestors_arr, a_obj);

                    snprintf(trace_buf, sizeof(trace_buf), "DFS Push: Discovered %s '%s' (depth %d) -> Pushed to Stack", 
                             label, parent_v->person->name, parent_dist);
                    cJSON_AddItemToArray(dsa_trace, cJSON_CreateString(trace_buf));

                    StackItem *next_item = (StackItem*)malloc(sizeof(StackItem));
                    next_item->v = parent_v;
                    next_item->distance = parent_dist;
                    stack_push(s, next_item);
                }
            }
            p_edge = p_edge->next;
        }
    }

    stack_destroy(s, false);
    hashmap_destroy(visited_dist, false);

    cJSON_AddItemToObject(res, "ancestors", ancestors_arr);
    cJSON_AddItemToObject(res, "dsa_trace", dsa_trace);
    cJSON_AddStringToObject(res, "algorithm_used", "Iterative DFS with manual LIFO Stack");
    return res;
}

cJSON* algo_get_descendants(const FamilyGraph *graph, const char *person_id) {
    if (!graph || !person_id) return NULL;

    GraphVertex *target_v = graph_get_vertex(graph, person_id);
    if (!target_v || !target_v->person) return NULL;

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "target_id", person_id);
    cJSON_AddStringToObject(res, "target_name", target_v->person->name);

    cJSON *descendants_arr = cJSON_CreateArray();
    cJSON *dsa_trace = cJSON_CreateArray();

    // Use custom Queue for BFS level-by-level traversal
    Queue *q = queue_create();
    HashMap *visited = hashmap_create(64);

    typedef struct QueueItem {
        GraphVertex *v;
        int distance;
    } QueueItem;

    QueueItem *init_item = (QueueItem*)malloc(sizeof(QueueItem));
    init_item->v = target_v;
    init_item->distance = 0;
    queue_enqueue(q, init_item);
    hashmap_put(visited, target_v->person->id, (void*)1);

    char trace_buf[256];
    snprintf(trace_buf, sizeof(trace_buf), "BFS Init: Enqueued target '%s' to custom Queue (level 0)", target_v->person->name);
    cJSON_AddItemToArray(dsa_trace, cJSON_CreateString(trace_buf));

    while (!queue_is_empty(q)) {
        QueueItem *item = (QueueItem*)queue_dequeue(q);
        GraphVertex *curr = item->v;
        int dist = item->distance;
        free(item);

        if (dist > 0 && curr->person) {
            snprintf(trace_buf, sizeof(trace_buf), "BFS Dequeue: Processed descendant '%s' at level %d", curr->person->name, dist);
            cJSON_AddItemToArray(dsa_trace, cJSON_CreateString(trace_buf));
        }

        // Enqueue children of curr
        Edge *c_edge = curr->children;
        while (c_edge) {
            GraphVertex *child_v = c_edge->target;
            if (child_v && child_v->person) {
                if (!hashmap_contains(visited, child_v->person->id)) {
                    hashmap_put(visited, child_v->person->id, (void*)1);
                    int child_dist = dist + 1;

                    cJSON *d_obj = cJSON_CreateObject();
                    cJSON_AddStringToObject(d_obj, "id", child_v->person->id);
                    cJSON_AddStringToObject(d_obj, "name", child_v->person->name);
                    cJSON_AddStringToObject(d_obj, "gender", child_v->person->gender);
                    cJSON_AddNumberToObject(d_obj, "distance", child_dist);

                    const char *label = "Descendant";
                    if (child_dist == 1) {
                        label = get_gender_label(child_v->person->gender, "Son", "Daughter", "Child");
                    } else if (child_dist == 2) {
                        label = get_gender_label(child_v->person->gender, "Grandson", "Granddaughter", "Grandchild");
                    } else if (child_dist == 3) {
                        label = get_gender_label(child_v->person->gender, "Great-Grandson", "Great-Granddaughter", "Great-Grandchild");
                    } else if (child_dist >= 4) {
                        label = "Great-Great-Grandchild";
                    }
                    cJSON_AddStringToObject(d_obj, "relationship_label", label);
                    cJSON_AddItemToArray(descendants_arr, d_obj);

                    snprintf(trace_buf, sizeof(trace_buf), "BFS Enqueue: Discovered %s '%s' (level %d) -> Enqueued", 
                             label, child_v->person->name, child_dist);
                    cJSON_AddItemToArray(dsa_trace, cJSON_CreateString(trace_buf));

                    QueueItem *next_item = (QueueItem*)malloc(sizeof(QueueItem));
                    next_item->v = child_v;
                    next_item->distance = child_dist;
                    queue_enqueue(q, next_item);
                }
            }
            c_edge = c_edge->next;
        }
    }

    queue_destroy(q, false);
    hashmap_destroy(visited, false);

    cJSON_AddItemToObject(res, "descendants", descendants_arr);
    cJSON_AddItemToObject(res, "dsa_trace", dsa_trace);
    cJSON_AddStringToObject(res, "algorithm_used", "Breadth-First Search (BFS) with manual FIFO Queue");
    return res;
}

cJSON* algo_find_common_ancestors(const FamilyGraph *graph, const char *id_a, const char *id_b) {
    if (!graph || !id_a || !id_b) return NULL;

    cJSON *anc_a_res = algo_get_ancestors(graph, id_a);
    cJSON *anc_b_res = algo_get_ancestors(graph, id_b);

    if (!anc_a_res || !anc_b_res) {
        if (anc_a_res) cJSON_Delete(anc_a_res);
        if (anc_b_res) cJSON_Delete(anc_b_res);
        return cJSON_CreateArray();
    }

    cJSON *anc_a_list = cJSON_GetObjectItem(anc_a_res, "ancestors");
    cJSON *anc_b_list = cJSON_GetObjectItem(anc_b_res, "ancestors");

    cJSON *common_arr = cJSON_CreateArray();

    // Iterate over ancestors of A and check if in ancestors of B
    int count_a = cJSON_GetArraySize(anc_a_list);
    int count_b = cJSON_GetArraySize(anc_b_list);

    for (int i = 0; i < count_a; i++) {
        cJSON *item_a = cJSON_GetArrayItem(anc_a_list, i);
        const char *a_id = cJSON_GetObjectItem(item_a, "id")->valuestring;
        int dist_a = cJSON_GetObjectItem(item_a, "distance")->valueint;

        for (int j = 0; j < count_b; j++) {
            cJSON *item_b = cJSON_GetArrayItem(anc_b_list, j);
            const char *b_id = cJSON_GetObjectItem(item_b, "id")->valuestring;
            int dist_b = cJSON_GetObjectItem(item_b, "distance")->valueint;

            if (strcmp(a_id, b_id) == 0) {
                cJSON *ca = cJSON_CreateObject();
                cJSON_AddStringToObject(ca, "id", a_id);
                cJSON_AddStringToObject(ca, "name", cJSON_GetObjectItem(item_a, "name")->valuestring);
                cJSON_AddNumberToObject(ca, "distance_from_a", dist_a);
                cJSON_AddNumberToObject(ca, "distance_from_b", dist_b);
                cJSON_AddNumberToObject(ca, "total_distance", dist_a + dist_b);
                cJSON_AddItemToArray(common_arr, ca);
                break;
            }
        }
    }

    cJSON_Delete(anc_a_res);
    cJSON_Delete(anc_b_res);
    return common_arr;
}

cJSON* algo_find_relationship_path(const FamilyGraph *graph, const char *id_a, const char *id_b) {
    if (!graph || !id_a || !id_b) return NULL;

    GraphVertex *va = graph_get_vertex(graph, id_a);
    GraphVertex *vb = graph_get_vertex(graph, id_b);
    if (!va || !vb) return NULL;

    cJSON *result = cJSON_CreateObject();
    cJSON_AddStringToObject(result, "from_id", id_a);
    cJSON_AddStringToObject(result, "from_name", va->person ? va->person->name : id_a);
    cJSON_AddStringToObject(result, "to_id", id_b);
    cJSON_AddStringToObject(result, "to_name", vb->person ? vb->person->name : id_b);

    if (strcmp(id_a, id_b) == 0) {
        cJSON_AddNumberToObject(result, "distance", 0);
        cJSON *steps = cJSON_CreateArray();
        cJSON *step = cJSON_CreateObject();
        cJSON_AddStringToObject(step, "node_id", id_a);
        cJSON_AddStringToObject(step, "node_name", va->person->name);
        cJSON_AddStringToObject(step, "action", "Same person");
        cJSON_AddItemToArray(steps, step);
        cJSON_AddItemToObject(result, "path", steps);
        return result;
    }

    // BFS on undirected relationship graph (parent, child, spouse)
    Queue *q = queue_create();
    HashMap *visited_paths = hashmap_create(127);

    PathNode *start_node = (PathNode*)calloc(1, sizeof(PathNode));
    strncpy(start_node->id, id_a, MAX_ID_LEN - 1);
    start_node->distance = 0;
    start_node->edge_label[0] = '\0';
    start_node->prev_id[0] = '\0';

    queue_enqueue(q, start_node);
    hashmap_put(visited_paths, id_a, start_node);

    bool found = false;

    while (!queue_is_empty(q)) {
        PathNode *curr = (PathNode*)queue_dequeue(q);
        if (strcmp(curr->id, id_b) == 0) {
            found = true;
            break;
        }

        GraphVertex *v = graph_get_vertex(graph, curr->id);
        if (!v) continue;

        // 1. Check parent edges (curr -> parent, relationship: "child_of")
        Edge *p_edge = v->parents;
        while (p_edge) {
            if (!hashmap_contains(visited_paths, p_edge->target_id)) {
                PathNode *pn = (PathNode*)calloc(1, sizeof(PathNode));
                strncpy(pn->id, p_edge->target_id, MAX_ID_LEN - 1);
                pn->distance = curr->distance + 1;
                strncpy(pn->edge_label, "child_of", sizeof(pn->edge_label) - 1);
                strncpy(pn->prev_id, curr->id, MAX_ID_LEN - 1);

                hashmap_put(visited_paths, p_edge->target_id, pn);
                queue_enqueue(q, pn);
            }
            p_edge = p_edge->next;
        }

        // 2. Check child edges (curr -> child, relationship: "parent_of")
        Edge *c_edge = v->children;
        while (c_edge) {
            if (!hashmap_contains(visited_paths, c_edge->target_id)) {
                PathNode *pn = (PathNode*)calloc(1, sizeof(PathNode));
                strncpy(pn->id, c_edge->target_id, MAX_ID_LEN - 1);
                pn->distance = curr->distance + 1;
                strncpy(pn->edge_label, "parent_of", sizeof(pn->edge_label) - 1);
                strncpy(pn->prev_id, curr->id, MAX_ID_LEN - 1);

                hashmap_put(visited_paths, c_edge->target_id, pn);
                queue_enqueue(q, pn);
            }
            c_edge = c_edge->next;
        }

        // 3. Check spouse edges (curr -> spouse, relationship: "spouse_of")
        Edge *s_edge = v->spouses;
        while (s_edge) {
            if (!hashmap_contains(visited_paths, s_edge->target_id)) {
                PathNode *pn = (PathNode*)calloc(1, sizeof(PathNode));
                strncpy(pn->id, s_edge->target_id, MAX_ID_LEN - 1);
                pn->distance = curr->distance + 1;
                strncpy(pn->edge_label, "spouse_of", sizeof(pn->edge_label) - 1);
                strncpy(pn->prev_id, curr->id, MAX_ID_LEN - 1);

                hashmap_put(visited_paths, s_edge->target_id, pn);
                queue_enqueue(q, pn);
            }
            s_edge = s_edge->next;
        }
    }

    queue_destroy(q, false);

    if (!found) {
        cJSON_AddNumberToObject(result, "distance", -1);
        cJSON_AddStringToObject(result, "status", "No connecting path found");
        cJSON_AddItemToObject(result, "path", cJSON_CreateArray());
        hashmap_destroy(visited_paths, true);
        return result;
    }

    // Reconstruct path backwards from id_b to id_a
    cJSON *steps = cJSON_CreateArray();
    const char *curr_id = id_b;
    PathNode *seq[256];
    int step_count = 0;

    while (curr_id && strlen(curr_id) > 0 && step_count < 256) {
        PathNode *pn = (PathNode*)hashmap_get(visited_paths, curr_id);
        if (!pn) break;
        seq[step_count++] = pn;
        if (strcmp(pn->id, id_a) == 0) break;
        curr_id = pn->prev_id;
    }

    // Add in forward order
    cJSON_AddNumberToObject(result, "distance", step_count - 1);
    for (int i = step_count - 1; i >= 0; i--) {
        PathNode *pn = seq[i];
        GraphVertex *v = graph_get_vertex(graph, pn->id);
        cJSON *step = cJSON_CreateObject();
        cJSON_AddStringToObject(step, "node_id", pn->id);
        cJSON_AddStringToObject(step, "node_name", (v && v->person) ? v->person->name : pn->id);
        cJSON_AddStringToObject(step, "edge_type", pn->edge_label);
        cJSON_AddItemToArray(steps, step);
    }
    cJSON_AddItemToObject(result, "path", steps);

    hashmap_destroy(visited_paths, true);
    return result;
}

cJSON* algo_classify_relationship(const FamilyGraph *graph, const char *id_a, const char *id_b) {
    if (!graph || !id_a || !id_b) return NULL;

    GraphVertex *va = graph_get_vertex(graph, id_a);
    GraphVertex *vb = graph_get_vertex(graph, id_b);
    if (!va || !vb) return NULL;

    GraphVertex *v = NULL;

    cJSON *res = cJSON_CreateObject();
    cJSON_AddStringToObject(res, "person_a_id", id_a);
    cJSON_AddStringToObject(res, "person_a_name", va->person ? va->person->name : id_a);
    cJSON_AddStringToObject(res, "person_b_id", id_b);
    cJSON_AddStringToObject(res, "person_b_name", vb->person ? vb->person->name : id_b);

    // 1. Same person
    if (strcmp(id_a, id_b) == 0) {
        cJSON_AddStringToObject(res, "relationship", "Self");
        cJSON_AddNumberToObject(res, "degree", 0);
        cJSON_AddItemToObject(res, "common_ancestors", cJSON_CreateArray());
        return res;
    }

    // 2. Direct Spouse
    if (graph_has_spouse(va, id_b)) {
        cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Husband", "Wife", "Spouse"));
        cJSON_AddNumberToObject(res, "degree", 1);
        cJSON_AddItemToObject(res, "common_ancestors", cJSON_CreateArray());
        return res;
    }

    // 3. Parent / Child
    if (graph_has_child(va, id_b)) {
        // B is child of A
        cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Son", "Daughter", "Child"));
        cJSON_AddNumberToObject(res, "degree", 1);
        cJSON_AddItemToObject(res, "common_ancestors", cJSON_CreateArray());
        return res;
    }
    if (graph_has_parent(va, id_b)) {
        // B is parent of A
        cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Father", "Mother", "Parent"));
        cJSON_AddNumberToObject(res, "degree", 1);
        cJSON_AddItemToObject(res, "common_ancestors", cJSON_CreateArray());
        return res;
    }

    // 4. Siblings (Full or Half)
    int shared_parents = 0;
    Edge *pa = va->parents;
    while (pa) {
        if (graph_has_parent(vb, pa->target_id)) {
            shared_parents++;
        }
        pa = pa->next;
    }
    if (shared_parents >= 2) {
        cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Brother", "Sister", "Sibling"));
        cJSON_AddNumberToObject(res, "degree", 2);
        cJSON_AddItemToObject(res, "common_ancestors", algo_find_common_ancestors(graph, id_a, id_b));
        return res;
    } else if (shared_parents == 1) {
        cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Half-Brother", "Half-Sister", "Half-Sibling"));
        cJSON_AddNumberToObject(res, "degree", 2);
        cJSON_AddItemToObject(res, "common_ancestors", algo_find_common_ancestors(graph, id_a, id_b));
        return res;
    }

    // 5. Grandparent / Grandchild
    // Is B a grandchild of A?
    Edge *ca = va->children;
    while (ca) {
        GraphVertex *child_a = ca->target;
        if (child_a && graph_has_child(child_a, id_b)) {
            cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Grandson", "Granddaughter", "Grandchild"));
            cJSON_AddNumberToObject(res, "degree", 2);
            cJSON_AddItemToObject(res, "common_ancestors", cJSON_CreateArray());
            return res;
        }
        ca = ca->next;
    }
    // Is B a grandparent of A?
    Edge *p_a = va->parents;
    while (p_a) {
        GraphVertex *parent_a = p_a->target;
        if (parent_a && graph_has_parent(parent_a, id_b)) {
            cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Grandfather", "Grandmother", "Grandparent"));
            cJSON_AddNumberToObject(res, "degree", 2);
            cJSON_AddItemToObject(res, "common_ancestors", cJSON_CreateArray());
            return res;
        }
        p_a = p_a->next;
    }

    // 6. Aunt / Uncle / Niece / Nephew
    // Is B an aunt/uncle of A? (B is sibling of A's parent)
    p_a = va->parents;
    while (p_a) {
        GraphVertex *parent_a = p_a->target;
        if (parent_a) {
            // Check if B and parent_a are siblings
            int p_shared = 0;
            Edge *p_parent = parent_a->parents;
            while (p_parent) {
                if (graph_has_parent(vb, p_parent->target_id)) p_shared++;
                p_parent = p_parent->next;
            }
            if (p_shared > 0) {
                cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Uncle", "Aunt", "Aunt/Uncle"));
                cJSON_AddNumberToObject(res, "degree", 3);
                cJSON_AddItemToObject(res, "common_ancestors", algo_find_common_ancestors(graph, id_a, id_b));
                return res;
            }
        }
        p_a = p_a->next;
    }

    // Is B a niece/nephew of A? (B is child of A's sibling)
    Edge *a_parents = va->parents;
    // Find siblings of A
    v = graph->vertices_head;
    while (v) {
        if (v != va && v != vb) {
            int sib_shared = 0;
            Edge *ap = a_parents;
            while (ap) {
                if (graph_has_parent(v, ap->target_id)) sib_shared++;
                ap = ap->next;
            }
            if (sib_shared > 0 && graph_has_child(v, id_b)) {
                cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Nephew", "Niece", "Niece/Nephew"));
                cJSON_AddNumberToObject(res, "degree", 3);
                cJSON_AddItemToObject(res, "common_ancestors", algo_find_common_ancestors(graph, id_a, id_b));
                return res;
            }
        }
        v = v->next;
    }

    // 7. First Cousins (A and B share grandparents)
    cJSON *ca_list = algo_find_common_ancestors(graph, id_a, id_b);
    if (cJSON_GetArraySize(ca_list) > 0) {
        cJSON *first_ca = cJSON_GetArrayItem(ca_list, 0);
        int d_a = cJSON_GetObjectItem(first_ca, "distance_from_a")->valueint;
        int d_b = cJSON_GetObjectItem(first_ca, "distance_from_b")->valueint;

        if (d_a == 2 && d_b == 2) {
            cJSON_AddStringToObject(res, "relationship", "First Cousin");
            cJSON_AddNumberToObject(res, "degree", 4);
            cJSON_AddItemToObject(res, "common_ancestors", ca_list);
            return res;
        } else if (d_a == 3 && d_b == 3) {
            cJSON_AddStringToObject(res, "relationship", "Second Cousin");
            cJSON_AddNumberToObject(res, "degree", 6);
            cJSON_AddItemToObject(res, "common_ancestors", ca_list);
            return res;
        }
    }

    // 8. In-law relationships
    // Spouse's Parent (Mother-in-law / Father-in-law)
    Edge *spa = va->spouses;
    while (spa) {
        if (graph_has_parent(spa->target, id_b)) {
            cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Father-in-law", "Mother-in-law", "Parent-in-law"));
            cJSON_AddNumberToObject(res, "degree", 2);
            cJSON_AddItemToObject(res, "common_ancestors", ca_list);
            return res;
        }
        spa = spa->next;
    }

    // Child's Spouse (Son-in-law / Daughter-in-law)
    Edge *cha = va->children;
    while (cha) {
        if (graph_has_spouse(cha->target, id_b)) {
            cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Son-in-law", "Daughter-in-law", "Child-in-law"));
            cJSON_AddNumberToObject(res, "degree", 2);
            cJSON_AddItemToObject(res, "common_ancestors", ca_list);
            return res;
        }
        cha = cha->next;
    }

    // Sibling's Spouse (Brother-in-law / Sister-in-law)
    v = graph->vertices_head;
    while (v) {
        if (v != va) {
            int sib = 0;
            Edge *p = va->parents;
            while (p) {
                if (graph_has_parent(v, p->target_id)) sib++;
                p = p->next;
            }
            if (sib > 0 && graph_has_spouse(v, id_b)) {
                cJSON_AddStringToObject(res, "relationship", get_gender_label(vb->person->gender, "Brother-in-law", "Sister-in-law", "Sibling-in-law"));
                cJSON_AddNumberToObject(res, "degree", 3);
                cJSON_AddItemToObject(res, "common_ancestors", ca_list);
                return res;
            }
        }
        v = v->next;
    }

    // 9. Fallback via BFS path
    cJSON *path_obj = algo_find_relationship_path(graph, id_a, id_b);
    int dist = cJSON_GetObjectItem(path_obj, "distance")->valueint;
    cJSON_Delete(path_obj);

    if (dist > 0) {
        char distant_buf[128];
        snprintf(distant_buf, sizeof(distant_buf), "Distant Relative (%d degrees of separation)", dist);
        cJSON_AddStringToObject(res, "relationship", distant_buf);
        cJSON_AddNumberToObject(res, "degree", dist);
    } else {
        cJSON_AddStringToObject(res, "relationship", "No recorded family connection (Unrelated)");
        cJSON_AddNumberToObject(res, "degree", -1);
    }

    cJSON_AddItemToObject(res, "common_ancestors", ca_list);
    return res;
}

bool algo_detect_cycle(const FamilyGraph *graph, const char *parent_id, const char *child_id) {
    if (!graph || !parent_id || !child_id) return false;
    if (strcmp(parent_id, child_id) == 0) return true;

    // Use iterative DFS with custom Stack to see if parent_id is reachable from child_id
    Stack *s = stack_create();
    HashMap *visited = hashmap_create(64);

    GraphVertex *start_v = graph_get_vertex(graph, child_id);
    if (!start_v) {
        stack_destroy(s, false);
        hashmap_destroy(visited, false);
        return false;
    }

    stack_push(s, start_v);
    hashmap_put(visited, child_id, (void*)1);

    bool cycle = false;

    while (!stack_is_empty(s)) {
        GraphVertex *curr = (GraphVertex*)stack_pop(s);
        if (strcmp(curr->person->id, parent_id) == 0) {
            cycle = true;
            break;
        }

        Edge *c = curr->children;
        while (c) {
            if (!hashmap_contains(visited, c->target_id)) {
                hashmap_put(visited, c->target_id, (void*)1);
                if (c->target) {
                    stack_push(s, c->target);
                }
            }
            c = c->next;
        }
    }

    stack_destroy(s, false);
    hashmap_destroy(visited, false);
    return cycle;
}
