#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "graph.h"
#include "storage.h"
#include "server.h"

static FamilyGraph *g_graph = NULL;

static void sigint_handler(int sig) {
    (void)sig;
    printf("\nGracefully shutting down KinGraph server...\n");
    server_stop();
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    const char *web_root = "KinGraph/frontend";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--web-root") == 0 && i + 1 < argc) {
            web_root = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("KinGraph Backend Server\n");
            printf("Usage: %s [options]\n", argv[0]);
            printf("  --port <port>        Port to listen on (default: 8080)\n");
            printf("  --web-root <dir>     Frontend directory (default: KinGraph/frontend)\n");
            printf("  -h, --help           Show this help message\n");
            return 0;
        }
    }

    signal(SIGINT, sigint_handler);
    signal(SIGTERM, sigint_handler);

    printf("Initializing KinGraph Data Structures...\n");
    g_graph = graph_create();
    if (!g_graph) {
        fprintf(stderr, "Fatal: failed to create graph data structure.\n");
        return 1;
    }

    // Try loading saved data; if unavailable or empty, load sample family dynasty
    char err_msg[256] = {0};
    bool loaded = storage_load(g_graph, NULL, NULL, err_msg, sizeof(err_msg));
    if (!loaded || g_graph->vertex_count == 0) {
        printf("No persistent database found. Loading initial sample family tree ('Montgomery-Sterling Dynasty')...\n");
        storage_load_sample_dataset(g_graph);
    } else {
        printf("Loaded %zu family members and %zu relationships from disk.\n", 
               g_graph->vertex_count, g_graph->edge_count_parent_child + g_graph->edge_count_spouse);
    }

    ServerConfig config;
    config.port = port;
    config.web_root = web_root;
    config.graph = g_graph;

    int res = server_start(&config);

    printf("Cleaning up graph and memory allocations...\n");
    if (g_graph) {
        graph_destroy(g_graph);
        g_graph = NULL;
    }

    printf("KinGraph server exited successfully.\n");
    return res;
}
