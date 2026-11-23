#ifndef SERVER_H
#define SERVER_H

#include "graph.h"
#include <stdbool.h>

#define DEFAULT_PORT 8080

typedef struct ServerConfig {
    int port;
    const char *web_root;
    FamilyGraph *graph;
} ServerConfig;

/**
 * Starts the HTTP server on configured port.
 * Blocks and accepts connections until interrupted (e.g. SIGINT).
 */
int server_start(ServerConfig *config);

/**
 * Signals server to stop.
 */
void server_stop(void);

#endif /* SERVER_H */
