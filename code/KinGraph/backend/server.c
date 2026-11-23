#include "server.h"
#include "algorithms.h"
#include "storage.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>

static volatile bool server_running = true;
static int server_socket_fd = -1;
static pthread_mutex_t graph_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct ClientThreadArgs {
    int client_fd;
    ServerConfig *config;
} ClientThreadArgs;

void server_stop(void) {
    server_running = false;
    if (server_socket_fd >= 0) {
        close(server_socket_fd);
        server_socket_fd = -1;
    }
}

static void send_response(int fd, int status_code, const char *status_text, 
                          const char *content_type, const char *body, size_t body_len) {
    char header_buf[1024];
    int header_len = snprintf(header_buf, sizeof(header_buf),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, status_text, content_type, body_len);

    write(fd, header_buf, header_len);
    if (body && body_len > 0) {
        write(fd, body, body_len);
    }
}

static void send_json(int fd, int status_code, cJSON *json_obj) {
    if (!json_obj) {
        const char *err = "{\"error\":\"Internal JSON Error\"}";
        send_response(fd, 500, "Internal Server Error", "application/json", err, strlen(err));
        return;
    }
    char *json_str = cJSON_PrintUnformatted(json_obj);
    size_t len = json_str ? strlen(json_str) : 0;
    const char *status_text = (status_code == 200) ? "OK" : 
                              (status_code == 201) ? "Created" :
                              (status_code == 400) ? "Bad Request" :
                              (status_code == 404) ? "Not Found" : "Error";
    send_response(fd, status_code, status_text, "application/json", json_str, len);
    if (json_str) free(json_str);
    cJSON_Delete(json_obj);
}

static void send_json_error(int fd, int status_code, const char *msg) {
    cJSON *err = cJSON_CreateObject();
    cJSON_AddBoolToObject(err, "success", false);
    cJSON_AddStringToObject(err, "error", msg ? msg : "Unknown error");
    send_json(fd, status_code, err);
}

static void send_json_success(int fd, const char *msg, cJSON *extra_data) {
    cJSON *res = extra_data ? extra_data : cJSON_CreateObject();
    cJSON_AddBoolToObject(res, "success", true);
    if (msg) cJSON_AddStringToObject(res, "message", msg);
    send_json(fd, 200, res);
}

static bool get_query_param(const char *query, const char *key, char *out_val, size_t out_len) {
    if (!query || !key || !out_val || out_len == 0) return false;

    char search_prefix[128];
    snprintf(search_prefix, sizeof(search_prefix), "%s=", key);

    const char *p = strstr(query, search_prefix);
    if (!p) return false;

    p += strlen(search_prefix);
    size_t i = 0;
    while (*p && *p != '&' && *p != ' ' && i < out_len - 1) {
        if (*p == '%' && *(p+1) && *(p+2)) {
            // URL decode simple hex
            char hex[3] = { *(p+1), *(p+2), 0 };
            out_val[i++] = (char)strtol(hex, NULL, 16);
            p += 3;
        } else if (*p == '+') {
            out_val[i++] = ' ';
            p++;
        } else {
            out_val[i++] = *p++;
        }
    }
    out_val[i] = '\0';
    return true;
}

static const char* get_mime_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0) return "text/html; charset=utf-8";
    if (strcasecmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcasecmp(ext, ".js") == 0) return "application/javascript; charset=utf-8";
    if (strcasecmp(ext, ".json") == 0) return "application/json";
    if (strcasecmp(ext, ".svg") == 0) return "image/svg+xml";
    if (strcasecmp(ext, ".png") == 0) return "image/png";
    if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcasecmp(ext, ".ico") == 0) return "image/x-icon";
    return "text/plain; charset=utf-8";
}

static void serve_static_file(int fd, const char *web_root, const char *url_path) {
    char filepath[1024];

    if (strcmp(url_path, "/") == 0 || strlen(url_path) == 0) {
        snprintf(filepath, sizeof(filepath), "%s/index.html", web_root);
    } else {
        // Prevent path traversal
        if (strstr(url_path, "..")) {
            const char *denied = "Access Denied";
            send_response(fd, 403, "Forbidden", "text/plain", denied, strlen(denied));
            return;
        }
        snprintf(filepath, sizeof(filepath), "%s%s", web_root, url_path);
    }

    FILE *f = fopen(filepath, "rb");
    if (!f) {
        const char *nf = "404 Not Found";
        send_response(fd, 404, "Not Found", "text/plain", nf, strlen(nf));
        return;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char*)malloc(size);
    if (!buf) {
        fclose(f);
        const char *oom = "Out of memory";
        send_response(fd, 500, "Internal Server Error", "text/plain", oom, strlen(oom));
        return;
    }

    size_t read_bytes = fread(buf, 1, size, f);
    fclose(f);

    send_response(fd, 200, "OK", get_mime_type(filepath), buf, read_bytes);
    free(buf);
}

static void handle_client_request(int client_fd, ServerConfig *config) {
    char req_buf[8192];
    ssize_t bytes_read = read(client_fd, req_buf, sizeof(req_buf) - 1);
    if (bytes_read <= 0) {
        close(client_fd);
        return;
    }
    req_buf[bytes_read] = '\0';

    // Parse HTTP line: METHOD PATH HTTP/1.x
    char method[16] = {0};
    char full_path[512] = {0};
    sscanf(req_buf, "%15s %511s", method, full_path);

    // Handle CORS preflight
    if (strcasecmp(method, "OPTIONS") == 0) {
        send_response(client_fd, 204, "No Content", "text/plain", "", 0);
        close(client_fd);
        return;
    }

    // Split path and query string
    char path[512] = {0};
    char query[512] = {0};
    char *q_ptr = strchr(full_path, '?');
    if (q_ptr) {
        size_t path_len = q_ptr - full_path;
        strncpy(path, full_path, path_len < sizeof(path) - 1 ? path_len : sizeof(path) - 1);
        strncpy(query, q_ptr + 1, sizeof(query) - 1);
    } else {
        strncpy(path, full_path, sizeof(path) - 1);
    }

    // Locate HTTP Body
    char *body_ptr = strstr(req_buf, "\r\n\r\n");
    if (body_ptr) body_ptr += 4;

    FamilyGraph *graph = config->graph;

    /* ---------------- API ROUTES ---------------- */
    if (strncmp(path, "/api/", 5) == 0) {

        // GET /api/stats
        if (strcmp(path, "/api/stats") == 0 && strcasecmp(method, "GET") == 0) {
            pthread_mutex_lock(&graph_lock);
            algo_compute_generations(graph);

            size_t total_keys = 0, collisions = 0, max_chain = 0;
            double load_factor = 0.0;
            hashmap_get_stats(graph->vertex_map, &total_keys, &collisions, &max_chain, &load_factor);

            int max_gen = 0;
            GraphVertex *v = graph->vertices_head;
            while (v) {
                if (v->person && v->person->generation > max_gen) {
                    max_gen = v->person->generation;
                }
                v = v->next;
            }

            cJSON *res = cJSON_CreateObject();
            cJSON_AddBoolToObject(res, "success", true);
            cJSON_AddNumberToObject(res, "total_people", graph->vertex_count);
            cJSON_AddNumberToObject(res, "total_parent_child", graph->edge_count_parent_child);
            cJSON_AddNumberToObject(res, "total_spouses", graph->edge_count_spouse);
            cJSON_AddNumberToObject(res, "generations_count", max_gen);

            cJSON *dsa_stats = cJSON_CreateObject();
            cJSON_AddNumberToObject(dsa_stats, "hash_table_size", total_keys);
            cJSON_AddNumberToObject(dsa_stats, "hash_table_capacity", graph->vertex_map->capacity);
            cJSON_AddNumberToObject(dsa_stats, "hash_collisions", collisions);
            cJSON_AddNumberToObject(dsa_stats, "hash_max_chain", max_chain);
            cJSON_AddNumberToObject(dsa_stats, "hash_load_factor", load_factor);
            cJSON_AddItemToObject(res, "dsa_metrics", dsa_stats);

            pthread_mutex_unlock(&graph_lock);
            send_json(client_fd, 200, res);
            close(client_fd);
            return;
        }

        // GET /api/family
        if (strcmp(path, "/api/family") == 0 && strcasecmp(method, "GET") == 0) {
            pthread_mutex_lock(&graph_lock);
            algo_compute_generations(graph);
            cJSON *tree_json = graph_to_json(graph);
            cJSON_AddBoolToObject(tree_json, "success", true);
            pthread_mutex_unlock(&graph_lock);
            send_json(client_fd, 200, tree_json);
            close(client_fd);
            return;
        }

        // GET /api/generations
        if (strcmp(path, "/api/generations") == 0 && strcasecmp(method, "GET") == 0) {
            pthread_mutex_lock(&graph_lock);
            cJSON *gen_json = algo_get_generations_json(graph);
            cJSON_AddBoolToObject(gen_json, "success", true);
            pthread_mutex_unlock(&graph_lock);
            send_json(client_fd, 200, gen_json);
            close(client_fd);
            return;
        }

        // GET /api/person?id=...
        if (strcmp(path, "/api/person") == 0 && strcasecmp(method, "GET") == 0) {
            char id[MAX_ID_LEN] = {0};
            if (!get_query_param(query, "id", id, sizeof(id))) {
                send_json_error(client_fd, 400, "Missing required query parameter 'id'.");
                close(client_fd);
                return;
            }

            pthread_mutex_lock(&graph_lock);
            GraphVertex *v = graph_get_vertex(graph, id);
            if (!v || !v->person) {
                pthread_mutex_unlock(&graph_lock);
                send_json_error(client_fd, 404, "Person not found.");
                close(client_fd);
                return;
            }

            cJSON *res = cJSON_CreateObject();
            cJSON_AddBoolToObject(res, "success", true);
            cJSON_AddItemToObject(res, "person", person_to_json(v->person));

            // Include parents, children, and spouses IDs
            cJSON *parents = cJSON_CreateArray();
            Edge *p = v->parents;
            while (p) {
                cJSON *item = cJSON_CreateObject();
                cJSON_AddStringToObject(item, "id", p->target_id);
                if (p->target && p->target->person) cJSON_AddStringToObject(item, "name", p->target->person->name);
                cJSON_AddItemToArray(parents, item);
                p = p->next;
            }
            cJSON_AddItemToObject(res, "parents", parents);

            cJSON *children = cJSON_CreateArray();
            Edge *c = v->children;
            while (c) {
                cJSON *item = cJSON_CreateObject();
                cJSON_AddStringToObject(item, "id", c->target_id);
                if (c->target && c->target->person) cJSON_AddStringToObject(item, "name", c->target->person->name);
                cJSON_AddItemToArray(children, item);
                c = c->next;
            }
            cJSON_AddItemToObject(res, "children", children);

            cJSON *spouses = cJSON_CreateArray();
            Edge *s = v->spouses;
            while (s) {
                cJSON *item = cJSON_CreateObject();
                cJSON_AddStringToObject(item, "id", s->target_id);
                if (s->target && s->target->person) cJSON_AddStringToObject(item, "name", s->target->person->name);
                cJSON_AddItemToArray(spouses, item);
                s = s->next;
            }
            cJSON_AddItemToObject(res, "spouses", spouses);

            pthread_mutex_unlock(&graph_lock);
            send_json(client_fd, 200, res);
            close(client_fd);
            return;
        }

        // POST /api/person (Create)
        if (strcmp(path, "/api/person") == 0 && strcasecmp(method, "POST") == 0) {
            cJSON *body_json = cJSON_Parse(body_ptr);
            if (!body_json) {
                send_json_error(client_fd, 400, "Invalid JSON payload in request body.");
                close(client_fd);
                return;
            }

            Person *p = person_from_json(body_json);
            if (!p) {
                cJSON_Delete(body_json);
                send_json_error(client_fd, 400, "Missing required fields (id and name).");
                close(client_fd);
                return;
            }

            char err_buf[256] = {0};
            if (!person_validate(p, err_buf, sizeof(err_buf))) {
                person_free(p);
                cJSON_Delete(body_json);
                send_json_error(client_fd, 400, err_buf);
                close(client_fd);
                return;
            }

            pthread_mutex_lock(&graph_lock);
            bool added = graph_add_person(graph, p, err_buf, sizeof(err_buf));
            if (!added) {
                person_free(p);
                pthread_mutex_unlock(&graph_lock);
                cJSON_Delete(body_json);
                send_json_error(client_fd, 400, err_buf);
                close(client_fd);
                return;
            }

            // Check if initial parent_id or spouse_id was provided in the creation payload
            cJSON *initial_parent = cJSON_GetObjectItemCaseSensitive(body_json, "parent_id");
            if (cJSON_IsString(initial_parent) && initial_parent->valuestring && strlen(initial_parent->valuestring) > 0) {
                graph_add_parent_child(graph, initial_parent->valuestring, p->id, NULL, 0);
            }
            cJSON *initial_spouse = cJSON_GetObjectItemCaseSensitive(body_json, "spouse_id");
            if (cJSON_IsString(initial_spouse) && initial_spouse->valuestring && strlen(initial_spouse->valuestring) > 0) {
                graph_add_spouse(graph, p->id, initial_spouse->valuestring, NULL, 0);
            }

            algo_compute_generations(graph);
            cJSON *resp_person = person_to_json(p);
            pthread_mutex_unlock(&graph_lock);

            cJSON_Delete(body_json);

            cJSON *res = cJSON_CreateObject();
            cJSON_AddItemToObject(res, "person", resp_person);
            send_json_success(client_fd, "Person added successfully to graph.", res);
            close(client_fd);
            return;
        }

        // PUT /api/person (Update)
        if (strcmp(path, "/api/person") == 0 && strcasecmp(method, "PUT") == 0) {
            cJSON *body_json = cJSON_Parse(body_ptr);
            if (!body_json) {
                send_json_error(client_fd, 400, "Invalid JSON payload in request body.");
                close(client_fd);
                return;
            }

            Person *p = person_from_json(body_json);
            cJSON_Delete(body_json);

            if (!p) {
                send_json_error(client_fd, 400, "Invalid person data.");
                close(client_fd);
                return;
            }

            char err_buf[256] = {0};
            pthread_mutex_lock(&graph_lock);
            bool updated = graph_update_person(graph, p, err_buf, sizeof(err_buf));
            person_free(p);
            pthread_mutex_unlock(&graph_lock);

            if (updated) {
                send_json_success(client_fd, "Person updated successfully.", NULL);
            } else {
                send_json_error(client_fd, 404, err_buf);
            }
            close(client_fd);
            return;
        }

        // DELETE /api/person?id=...
        if (strcmp(path, "/api/person") == 0 && strcasecmp(method, "DELETE") == 0) {
            char id[MAX_ID_LEN] = {0};
            if (!get_query_param(query, "id", id, sizeof(id))) {
                send_json_error(client_fd, 400, "Missing required query parameter 'id'.");
                close(client_fd);
                return;
            }

            char err_buf[256] = {0};
            pthread_mutex_lock(&graph_lock);
            bool removed = graph_remove_person(graph, id, err_buf, sizeof(err_buf));
            if (removed) {
                algo_compute_generations(graph);
            }
            pthread_mutex_unlock(&graph_lock);

            if (removed) {
                send_json_success(client_fd, "Person deleted and all graph connections updated.", NULL);
            } else {
                send_json_error(client_fd, 404, err_buf);
            }
            close(client_fd);
            return;
        }

        // POST /api/relationship (Connect parent-child or spouse)
        if (strcmp(path, "/api/relationship") == 0 && strcasecmp(method, "POST") == 0) {
            cJSON *body_json = cJSON_Parse(body_ptr);
            if (!body_json) {
                send_json_error(client_fd, 400, "Invalid JSON payload.");
                close(client_fd);
                return;
            }

            cJSON *j_type = cJSON_GetObjectItem(body_json, "type");
            const char *type = (cJSON_IsString(j_type) && j_type->valuestring) ? j_type->valuestring : "parent_child";

            char err_buf[256] = {0};
            bool ok = false;

            pthread_mutex_lock(&graph_lock);
            if (strcasecmp(type, "parent_child") == 0) {
                cJSON *j_p = cJSON_GetObjectItem(body_json, "parent_id");
                cJSON *j_c = cJSON_GetObjectItem(body_json, "child_id");
                if (!cJSON_IsString(j_p) || !cJSON_IsString(j_c)) {
                    pthread_mutex_unlock(&graph_lock);
                    cJSON_Delete(body_json);
                    send_json_error(client_fd, 400, "Missing parent_id or child_id.");
                    close(client_fd);
                    return;
                }
                ok = graph_add_parent_child(graph, j_p->valuestring, j_c->valuestring, err_buf, sizeof(err_buf));
            } else if (strcasecmp(type, "spouse") == 0) {
                cJSON *j_a = cJSON_GetObjectItem(body_json, "person_a");
                cJSON *j_b = cJSON_GetObjectItem(body_json, "person_b");
                if (!cJSON_IsString(j_a) || !cJSON_IsString(j_b)) {
                    pthread_mutex_unlock(&graph_lock);
                    cJSON_Delete(body_json);
                    send_json_error(client_fd, 400, "Missing person_a or person_b.");
                    close(client_fd);
                    return;
                }
                ok = graph_add_spouse(graph, j_a->valuestring, j_b->valuestring, err_buf, sizeof(err_buf));
            } else {
                snprintf(err_buf, sizeof(err_buf), "Unknown relationship type '%s'.", type);
            }

            if (ok) {
                algo_compute_generations(graph);
            }
            pthread_mutex_unlock(&graph_lock);
            cJSON_Delete(body_json);

            if (ok) {
                send_json_success(client_fd, "Relationship connected successfully.", NULL);
            } else {
                send_json_error(client_fd, 400, err_buf);
            }
            close(client_fd);
            return;
        }

        // DELETE /api/relationship
        if (strcmp(path, "/api/relationship") == 0 && strcasecmp(method, "DELETE") == 0) {
            cJSON *body_json = cJSON_Parse(body_ptr);
            if (!body_json) {
                send_json_error(client_fd, 400, "Invalid JSON payload.");
                close(client_fd);
                return;
            }

            cJSON *j_type = cJSON_GetObjectItem(body_json, "type");
            const char *type = (cJSON_IsString(j_type) && j_type->valuestring) ? j_type->valuestring : "parent_child";

            char err_buf[256] = {0};
            bool ok = false;

            pthread_mutex_lock(&graph_lock);
            if (strcasecmp(type, "parent_child") == 0) {
                cJSON *j_p = cJSON_GetObjectItem(body_json, "parent_id");
                cJSON *j_c = cJSON_GetObjectItem(body_json, "child_id");
                if (cJSON_IsString(j_p) && cJSON_IsString(j_c)) {
                    ok = graph_remove_parent_child(graph, j_p->valuestring, j_c->valuestring, err_buf, sizeof(err_buf));
                }
            } else if (strcasecmp(type, "spouse") == 0) {
                cJSON *j_a = cJSON_GetObjectItem(body_json, "person_a");
                cJSON *j_b = cJSON_GetObjectItem(body_json, "person_b");
                if (cJSON_IsString(j_a) && cJSON_IsString(j_b)) {
                    ok = graph_remove_spouse(graph, j_a->valuestring, j_b->valuestring, err_buf, sizeof(err_buf));
                }
            }
            if (ok) {
                algo_compute_generations(graph);
            }
            pthread_mutex_unlock(&graph_lock);
            cJSON_Delete(body_json);

            if (ok) {
                send_json_success(client_fd, "Relationship removed successfully.", NULL);
            } else {
                send_json_error(client_fd, 400, err_buf[0] ? err_buf : "Failed to remove relationship.");
            }
            close(client_fd);
            return;
        }

        // GET /api/ancestors?id=...
        if (strcmp(path, "/api/ancestors") == 0 && strcasecmp(method, "GET") == 0) {
            char id[MAX_ID_LEN] = {0};
            if (!get_query_param(query, "id", id, sizeof(id))) {
                send_json_error(client_fd, 400, "Missing required query parameter 'id'.");
                close(client_fd);
                return;
            }

            pthread_mutex_lock(&graph_lock);
            cJSON *anc = algo_get_ancestors(graph, id);
            pthread_mutex_unlock(&graph_lock);

            if (anc) {
                cJSON_AddBoolToObject(anc, "success", true);
                send_json(client_fd, 200, anc);
            } else {
                send_json_error(client_fd, 404, "Person not found in family graph.");
            }
            close(client_fd);
            return;
        }

        // GET /api/descendants?id=...
        if (strcmp(path, "/api/descendants") == 0 && strcasecmp(method, "GET") == 0) {
            char id[MAX_ID_LEN] = {0};
            if (!get_query_param(query, "id", id, sizeof(id))) {
                send_json_error(client_fd, 400, "Missing required query parameter 'id'.");
                close(client_fd);
                return;
            }

            pthread_mutex_lock(&graph_lock);
            cJSON *desc = algo_get_descendants(graph, id);
            pthread_mutex_unlock(&graph_lock);

            if (desc) {
                cJSON_AddBoolToObject(desc, "success", true);
                send_json(client_fd, 200, desc);
            } else {
                send_json_error(client_fd, 404, "Person not found in family graph.");
            }
            close(client_fd);
            return;
        }

        // GET /api/relationship?a=...&b=...
        if (strcmp(path, "/api/relationship") == 0 && strcasecmp(method, "GET") == 0) {
            char a_id[MAX_ID_LEN] = {0};
            char b_id[MAX_ID_LEN] = {0};
            if (!get_query_param(query, "a", a_id, sizeof(a_id)) || !get_query_param(query, "b", b_id, sizeof(b_id))) {
                send_json_error(client_fd, 400, "Missing parameters 'a' or 'b'.");
                close(client_fd);
                return;
            }

            pthread_mutex_lock(&graph_lock);
            cJSON *rel = algo_classify_relationship(graph, a_id, b_id);
            cJSON *path_obj = algo_find_relationship_path(graph, a_id, b_id);
            if (path_obj && rel) {
                cJSON *steps = cJSON_DetachItemFromObject(path_obj, "path");
                cJSON_AddItemToObject(rel, "path", steps);
                cJSON_Delete(path_obj);
            }
            pthread_mutex_unlock(&graph_lock);

            if (rel) {
                cJSON_AddBoolToObject(rel, "success", true);
                send_json(client_fd, 200, rel);
            } else {
                send_json_error(client_fd, 404, "One or both people not found.");
            }
            close(client_fd);
            return;
        }

        // POST /api/save
        if (strcmp(path, "/api/save") == 0 && strcasecmp(method, "POST") == 0) {
            pthread_mutex_lock(&graph_lock);
            bool saved = storage_save(graph, NULL, NULL);
            pthread_mutex_unlock(&graph_lock);

            if (saved) {
                send_json_success(client_fd, "Family data successfully persisted to disk.", NULL);
            } else {
                send_json_error(client_fd, 500, "Failed to write data files.");
            }
            close(client_fd);
            return;
        }

        // POST /api/load
        if (strcmp(path, "/api/load") == 0 && strcasecmp(method, "POST") == 0) {
            pthread_mutex_lock(&graph_lock);
            // Clear current graph and reload
            char err_buf[256] = {0};
            // Destroy existing vertices
            while (graph->vertices_head) {
                graph_remove_person(graph, graph->vertices_head->person->id, NULL, 0);
            }
            bool loaded = storage_load(graph, NULL, NULL, err_buf, sizeof(err_buf));
            pthread_mutex_unlock(&graph_lock);

            if (loaded) {
                send_json_success(client_fd, "Family tree loaded from persistent storage.", NULL);
            } else {
                send_json_error(client_fd, 500, err_buf[0] ? err_buf : "Failed to load data.");
            }
            close(client_fd);
            return;
        }

        // POST /api/reset
        if (strcmp(path, "/api/reset") == 0 && strcasecmp(method, "POST") == 0) {
            pthread_mutex_lock(&graph_lock);
            while (graph->vertices_head) {
                graph_remove_person(graph, graph->vertices_head->person->id, NULL, 0);
            }
            storage_load_sample_dataset(graph);
            pthread_mutex_unlock(&graph_lock);

            send_json_success(client_fd, "Reset to rich sample dataset (Montgomery-Sterling Dynasty).", NULL);
            close(client_fd);
            return;
        }

        // Unknown API route
        send_json_error(client_fd, 404, "Endpoint not found.");
        close(client_fd);
        return;
    }

    /* ---------------- STATIC FILE SERVING ---------------- */
    serve_static_file(client_fd, config->web_root, path);
    close(client_fd);
}

static void* client_thread_func(void *arg) {
    ClientThreadArgs *cta = (ClientThreadArgs*)arg;
    handle_client_request(cta->client_fd, cta->config);
    free(cta);
    return NULL;
}

int server_start(ServerConfig *config) {
    if (!config) return -1;

    signal(SIGPIPE, SIG_IGN); // Ignore broken pipe when clients disconnect early

    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_fd < 0) {
        perror("socket creation failed");
        return -1;
    }

    int opt = 1;
    setsockopt(server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(config->port);

    if (bind(server_socket_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        fprintf(stderr, "\n❌ [ERROR] Port %d is already in use by another process!\n", config->port);
        fprintf(stderr, "   • Your KinGraph server may already be running: try visiting http://localhost:%d\n", config->port);
        fprintf(stderr, "   • To kill the previous process on port %d, run: kill $(lsof -t -i :%d)\n", config->port, config->port);
        fprintf(stderr, "   • Or launch on a different port: ./kingraph_server --port %d --web-root %s\n\n", config->port + 1, config->web_root);
        close(server_socket_fd);
        return -1;
    }

    if (listen(server_socket_fd, 64) < 0) {
        perror("socket listen failed");
        close(server_socket_fd);
        return -1;
    }

    printf("\n======================================================\n");
    printf("  🚀 KinGraph C Backend Server Running\n");
    printf("  Port:     %d\n", config->port);
    printf("  Web Root: %s\n", config->web_root);
    printf("  URL:      http://localhost:%d\n", config->port);
    printf("======================================================\n\n");

    while (server_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_socket_fd, (struct sockaddr*)&client_addr, &client_len);

        if (client_fd < 0) {
            if (!server_running) break;
            continue;
        }

        ClientThreadArgs *cta = (ClientThreadArgs*)malloc(sizeof(ClientThreadArgs));
        if (!cta) {
            close(client_fd);
            continue;
        }
        cta->client_fd = client_fd;
        cta->config = config;

        pthread_t tid;
        if (pthread_create(&tid, NULL, client_thread_func, cta) == 0) {
            pthread_detach(tid);
        } else {
            close(client_fd);
            free(cta);
        }
    }

    if (server_socket_fd >= 0) {
        close(server_socket_fd);
        server_socket_fd = -1;
    }

    return 0;
}
