#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "person.h"
#include "hashmap.h"
#include "queue.h"
#include "stack.h"
#include "graph.h"
#include "algorithms.h"
#include "storage.h"

static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (cond) { \
        printf("  [PASS] %s\n", msg); \
        g_tests_passed++; \
    } else { \
        printf("  [FAIL] %s (Line %d)\n", msg, __LINE__); \
        g_tests_failed++; \
    } \
} while(0)

void test_person(void) {
    printf("\n--- Test Suite 1: Person Management ---\n");
    Person *p = person_create("T01", "Alan Turing", "Male", 1912, 1954, 0, "Mathematician");
    TEST_ASSERT(p != NULL, "Person creation succeeds");
    TEST_ASSERT(strcmp(p->id, "T01") == 0, "Person ID matches");
    TEST_ASSERT(strcmp(p->name, "Alan Turing") == 0, "Person Name matches");

    char err[128];
    TEST_ASSERT(person_validate(p, err, sizeof(err)), "Person validation passes for valid person");

    // Invalid person: death year before birth year
    Person *invalid = person_create("T02", "Time Traveler", "Other", 2000, 1990, 0, "");
    TEST_ASSERT(!person_validate(invalid, err, sizeof(err)), "Person validation fails when death_year < birth_year");
    person_free(invalid);

    // Deep copy
    Person *clone = person_clone(p);
    TEST_ASSERT(clone != NULL && strcmp(clone->name, p->name) == 0, "Person cloning creates identical copy");
    person_free(clone);
    person_free(p);
}

void test_hashmap(void) {
    printf("\n--- Test Suite 2: Hash Table with Chaining ---\n");
    HashMap *map = hashmap_create(7); // Small capacity to force collisions
    TEST_ASSERT(map != NULL, "HashMap creation succeeds");

    int v1 = 100, v2 = 200, v3 = 300, v4 = 400;
    hashmap_put(map, "id_alpha", &v1);
    hashmap_put(map, "id_beta", &v2);
    hashmap_put(map, "id_gamma", &v3);
    hashmap_put(map, "id_delta", &v4);

    TEST_ASSERT(hashmap_size(map) == 4, "HashMap size is 4");
    TEST_ASSERT(*(int*)hashmap_get(map, "id_alpha") == 100, "Retrieve 'id_alpha'");
    TEST_ASSERT(*(int*)hashmap_get(map, "id_gamma") == 300, "Retrieve 'id_gamma'");
    TEST_ASSERT(hashmap_get(map, "id_nonexistent") == NULL, "Nonexistent key returns NULL");

    // Update existing key
    int v1_new = 999;
    hashmap_put(map, "id_alpha", &v1_new);
    TEST_ASSERT(*(int*)hashmap_get(map, "id_alpha") == 999, "Update existing key");
    TEST_ASSERT(hashmap_size(map) == 4, "Size unchanged after update");

    // Check collision metrics
    size_t total = 0, collisions = 0, max_chain = 0;
    double load_factor = 0.0;
    hashmap_get_stats(map, &total, &collisions, &max_chain, &load_factor);
    TEST_ASSERT(max_chain >= 1, "Collision chaining tracks chain lengths");
    printf("    -> Stats: Keys=%zu, Collisions=%zu, MaxChain=%zu, LoadFactor=%.2f\n", 
           total, collisions, max_chain, load_factor);

    // Remove key
    void *rem = hashmap_remove(map, "id_beta");
    TEST_ASSERT(rem == &v2, "Removed key returns expected pointer");
    TEST_ASSERT(!hashmap_contains(map, "id_beta"), "Key no longer exists after remove");
    TEST_ASSERT(hashmap_size(map) == 3, "Size decremented to 3");

    hashmap_destroy(map, false);
}

void test_queue(void) {
    printf("\n--- Test Suite 3: Dynamic FIFO Queue ---\n");
    Queue *q = queue_create();
    TEST_ASSERT(q != NULL, "Queue created");
    TEST_ASSERT(queue_is_empty(q), "New queue is empty");

    int a = 1, b = 2, c = 3;
    queue_enqueue(q, &a);
    queue_enqueue(q, &b);
    queue_enqueue(q, &c);

    TEST_ASSERT(queue_size(q) == 3, "Queue size is 3");
    TEST_ASSERT(*(int*)queue_peek(q) == 1, "Queue peek returns first enqueued element");
    TEST_ASSERT(*(int*)queue_dequeue(q) == 1, "First dequeue is 1 (FIFO)");
    TEST_ASSERT(*(int*)queue_dequeue(q) == 2, "Second dequeue is 2 (FIFO)");
    TEST_ASSERT(*(int*)queue_dequeue(q) == 3, "Third dequeue is 3 (FIFO)");
    TEST_ASSERT(queue_is_empty(q), "Queue is empty after 3 dequeues");

    queue_destroy(q, false);
}

void test_stack(void) {
    printf("\n--- Test Suite 4: Dynamic LIFO Stack ---\n");
    Stack *s = stack_create();
    TEST_ASSERT(s != NULL, "Stack created");
    TEST_ASSERT(stack_is_empty(s), "New stack is empty");

    int a = 10, b = 20, c = 30;
    stack_push(s, &a);
    stack_push(s, &b);
    stack_push(s, &c);

    TEST_ASSERT(stack_size(s) == 3, "Stack size is 3");
    TEST_ASSERT(*(int*)stack_peek(s) == 30, "Stack peek returns top element (30)");
    TEST_ASSERT(*(int*)stack_pop(s) == 30, "First pop is 30 (LIFO)");
    TEST_ASSERT(*(int*)stack_pop(s) == 20, "Second pop is 20 (LIFO)");
    TEST_ASSERT(*(int*)stack_pop(s) == 10, "Third pop is 10 (LIFO)");
    TEST_ASSERT(stack_is_empty(s), "Stack is empty after 3 pops");

    stack_destroy(s, false);
}

void test_graph_and_cycle_detection(void) {
    printf("\n--- Test Suite 5: Graph Adjacency List & Cycle Detection ---\n");
    FamilyGraph *g = graph_create();
    TEST_ASSERT(g != NULL, "Graph created");

    char err[256];
    graph_add_person(g, person_create("G1", "Grandparent", "Female", 1930, 0, 1, ""), err, sizeof(err));
    graph_add_person(g, person_create("P1", "Parent", "Male", 1960, 0, 1, ""), err, sizeof(err));
    graph_add_person(g, person_create("C1", "Child", "Female", 1990, 0, 1, ""), err, sizeof(err));

    TEST_ASSERT(g->vertex_count == 3, "Added 3 people to graph");

    // Connect G1 -> P1, P1 -> C1
    TEST_ASSERT(graph_add_parent_child(g, "G1", "P1", err, sizeof(err)), "Add Parent-Child G1 -> P1");
    TEST_ASSERT(graph_add_parent_child(g, "P1", "C1", err, sizeof(err)), "Add Parent-Child P1 -> C1");

    // Attempt to add C1 -> G1 (should FAIL due to Cycle Detection!)
    bool cycle_result = graph_add_parent_child(g, "C1", "G1", err, sizeof(err));
    TEST_ASSERT(!cycle_result, "Cycle detection prevents descendant C1 from becoming parent of ancestor G1");
    printf("    -> Rejected Cycle Message: \"%s\"\n", err);

    // Attempt self-parent (C1 -> C1)
    bool self_cycle = graph_add_parent_child(g, "C1", "C1", err, sizeof(err));
    TEST_ASSERT(!self_cycle, "Self-parenting rejected");

    // Add spouse
    graph_add_person(g, person_create("P2", "Parent Spouse", "Female", 1962, 0, 1, ""), err, sizeof(err));
    TEST_ASSERT(graph_add_spouse(g, "P1", "P2", err, sizeof(err)), "Add mutual spouse P1 <-> P2");
    TEST_ASSERT(graph_has_spouse(graph_get_vertex(g, "P1"), "P2"), "P1 has spouse P2");
    TEST_ASSERT(graph_has_spouse(graph_get_vertex(g, "P2"), "P1"), "P2 has spouse P1");

    graph_destroy(g);
}

void test_deletion_cascade(void) {
    printf("\n--- Test Suite 6: Member Deletion & Consistency Maintenance ---\n");
    FamilyGraph *g = graph_create();
    char err[256];

    graph_add_person(g, person_create("A", "Person A", "Male", 1950, 0, 1, ""), err, sizeof(err));
    graph_add_person(g, person_create("B", "Person B", "Female", 1952, 0, 1, ""), err, sizeof(err));
    graph_add_person(g, person_create("C", "Person C", "Male", 1980, 0, 1, ""), err, sizeof(err));

    graph_add_spouse(g, "A", "B", err, sizeof(err));
    graph_add_parent_child(g, "A", "C", err, sizeof(err));
    graph_add_parent_child(g, "B", "C", err, sizeof(err));

    TEST_ASSERT(g->vertex_count == 3, "Initial 3 members");
    TEST_ASSERT(g->edge_count_parent_child == 2, "2 parent-child edges");
    TEST_ASSERT(g->edge_count_spouse == 1, "1 spouse edge pair");

    // Delete Person A
    bool del_ok = graph_remove_person(g, "A", err, sizeof(err));
    TEST_ASSERT(del_ok, "Deleted Person A");
    TEST_ASSERT(g->vertex_count == 2, "Vertex count decremented to 2");
    TEST_ASSERT(graph_get_person(g, "A") == NULL, "Person A no longer found in Hash Table");

    // Verify Person B no longer lists A as spouse
    GraphVertex *vb = graph_get_vertex(g, "B");
    TEST_ASSERT(!graph_has_spouse(vb, "A"), "Person B spouse reference to A cleanly removed");

    // Verify Person C no longer lists A as parent
    GraphVertex *vc = graph_get_vertex(g, "C");
    TEST_ASSERT(!graph_has_parent(vc, "A"), "Person C parent reference to A cleanly removed");
    TEST_ASSERT(graph_has_parent(vc, "B"), "Person C still correctly retains parent B");

    graph_destroy(g);
}

void test_algorithms_and_pathfinding(void) {
    printf("\n--- Test Suite 7: BFS Generations, Descendants, DFS Ancestors & Relationship Paths ---\n");
    FamilyGraph *g = graph_create();
    storage_load_sample_dataset(g);

    // 1. Generations computation
    algo_compute_generations(g);
    Person *p_arthur = graph_get_person(g, "P01");
    Person *p_julian = graph_get_person(g, "P03");
    Person *p_lucas = graph_get_person(g, "P08");
    Person *p_leo = graph_get_person(g, "P14");

    TEST_ASSERT(p_arthur && p_arthur->generation == 1, "Arthur Sterling is Generation 1 (Founder)");
    TEST_ASSERT(p_julian && p_julian->generation == 2, "Julian Sterling is Generation 2 (Son)");
    TEST_ASSERT(p_lucas && p_lucas->generation == 3, "Lucas Sterling is Generation 3 (Grandson)");
    TEST_ASSERT(p_leo && p_leo->generation == 4, "Leo Sterling is Generation 4 (Great-grandson)");

    // 2. DFS Ancestors of Lucas (P08)
    cJSON *anc_res = algo_get_ancestors(g, "P08");
    cJSON *anc_arr = cJSON_GetObjectItem(anc_res, "ancestors");
    int anc_count = cJSON_GetArraySize(anc_arr);
    TEST_ASSERT(anc_count >= 4, "Lucas has at least 4 recorded ancestors (Julian, Clara, Arthur, Eleanor)");
    cJSON_Delete(anc_res);

    // 3. BFS Descendants of Arthur (P01)
    cJSON *desc_res = algo_get_descendants(g, "P01");
    cJSON *desc_arr = cJSON_GetObjectItem(desc_res, "descendants");
    int desc_count = cJSON_GetArraySize(desc_arr);
    TEST_ASSERT(desc_count >= 8, "Arthur has multiple descendants across generations");
    cJSON_Delete(desc_res);

    // 4. Relationship Classification
    // Arthur (P01) to Lucas (P08) -> Grandson
    cJSON *rel_ag = algo_classify_relationship(g, "P01", "P08");
    const char *rel_name1 = cJSON_GetObjectItem(rel_ag, "relationship")->valuestring;
    TEST_ASSERT(strcmp(rel_name1, "Grandson") == 0, "Arthur -> Lucas classified as 'Grandson'");
    cJSON_Delete(rel_ag);

    // Julian (P03) and Beatrice (P05) -> Siblings
    cJSON *rel_jb = algo_classify_relationship(g, "P03", "P05");
    const char *rel_name2 = cJSON_GetObjectItem(rel_jb, "relationship")->valuestring;
    TEST_ASSERT(strcmp(rel_name2, "Sister") == 0, "Julian -> Beatrice classified as 'Sister'");
    cJSON_Delete(rel_jb);

    // Lucas (P08) and Sophia (P10) -> First Cousins
    cJSON *rel_ls = algo_classify_relationship(g, "P08", "P10");
    const char *rel_name3 = cJSON_GetObjectItem(rel_ls, "relationship")->valuestring;
    TEST_ASSERT(strcmp(rel_name3, "First Cousin") == 0, "Lucas and Sophia classified as 'First Cousin'");
    cJSON_Delete(rel_ls);

    // 5. Shortest path finding
    cJSON *path_obj = algo_find_relationship_path(g, "P08", "P10");
    int path_dist = cJSON_GetObjectItem(path_obj, "distance")->valueint;
    TEST_ASSERT(path_dist > 0, "Found connecting path between first cousins Lucas & Sophia");
    cJSON_Delete(path_obj);

    graph_destroy(g);
}

void test_persistence(void) {
    printf("\n--- Test Suite 8: File Persistence (C File Handling) ---\n");
    const char *test_p_file = "/tmp/test_kingraph_people.dat";
    const char *test_r_file = "/tmp/test_kingraph_rel.dat";

    FamilyGraph *g1 = graph_create();
    storage_load_sample_dataset(g1);
    size_t orig_people = g1->vertex_count;
    size_t orig_edges = g1->edge_count_parent_child;

    bool saved = storage_save(g1, test_p_file, test_r_file);
    TEST_ASSERT(saved, "Successfully saved family graph to C files");
    graph_destroy(g1);

    // Reload into a fresh graph
    FamilyGraph *g2 = graph_create();
    char err[256];
    bool loaded = storage_load(g2, test_p_file, test_r_file, err, sizeof(err));
    TEST_ASSERT(loaded, "Successfully loaded family graph from C files");
    TEST_ASSERT(g2->vertex_count == orig_people, "Loaded person count matches saved count");
    TEST_ASSERT(g2->edge_count_parent_child == orig_edges, "Loaded parent-child count matches saved count");

    graph_destroy(g2);
    remove(test_p_file);
    remove(test_r_file);
}

int main(void) {
    printf("=================================================================\n");
    printf("    KinGraph – Automated C Data Structures & Algorithms Test\n");
    printf("=================================================================\n");

    test_person();
    test_hashmap();
    test_queue();
    test_stack();
    test_graph_and_cycle_detection();
    test_deletion_cascade();
    test_algorithms_and_pathfinding();
    test_persistence();

    printf("\n=================================================================\n");
    printf("  TEST RESULTS: %d Passed, %d Failed\n", g_tests_passed, g_tests_failed);
    printf("=================================================================\n\n");

    return (g_tests_failed == 0) ? 0 : 1;
}
