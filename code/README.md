# KinGraph — Family Tree Builder & Relationship Analysis System
**A Modern Genealogy Web Application Powered Purely by Custom C Data Structures & Algorithms**

---

## 1. Project Overview

**KinGraph** is an academic-grade, full-stack genealogy and kinship analysis platform. Designed as a college Data Structures & Algorithms (DSA) project, it models complex human family networks using fundamental computer science concepts implemented strictly in the **C programming language**.

Unlike commercial family tree drawing utilities that restrict genealogy to naive hierarchical trees, KinGraph views kinship through **Directed Graph Theory**. It exposes a high-performance C HTTP engine communicating via JSON with a modern, reactive SaaS-style web interface.

---

## 2. Problem Statement

Modeling real-world genealogical networks in traditional software presents critical computer science hurdles:
1. **Convergence Paradox**: Human reproduction requires two parents. Strict binary or $n$-ary trees allow only a single parent per node.
2. **Cycle Prevention**: Biological lineage cannot form closed loops (e.g., a descendant cannot be the biological parent of an ancestor). Naive graph representations risk temporal paradoxes without rigorous cycle detection.
3. **Orthogonal Relationships**: Marriage/partnership is a mutual bidirectional horizontal bond, fundamentally different from directed vertical ancestor-descendant edges.
4. **Graph Traversal Efficiency**: Discovering kinship, Lowest Common Ancestors (LCA), and genealogical distance requires optimal graph search algorithms ($O(V+E)$).

---

## 3. Objectives

- **Pedagogical Purity**: Implement all data structures (Graph, Hash Table, Queue, Stack) and graph algorithms (BFS, DFS, Cycle Detection, Pathfinding) manually in C without C++, STL, Python, Java, or external runtime dependencies.
- **Persistent C File Handling**: Store records and relationships directly on disk using custom text/binary formats with zero SQL or NoSQL database dependencies.
- **Interactive Visual SaaS Frontend**: Provide a sleek, dark-mode SVG visualization supporting real-time panning, zooming, generation banding, and algorithmic highlighting.
- **Zero-Glitch API Bridge**: Serve static assets and JSON REST endpoints through a POSIX socket HTTP server in C.

---

## 4. Technologies Used

| Layer | Technologies | Purpose |
|---|---|---|
| **Core Backend & DSA** | C (C99 / C11, Clang / GCC) | Manual memory allocation (`malloc`/`free`), pointer manipulation, data structures, and algorithms |
| **Networking & HTTP** | POSIX Sockets (`sys/socket.h`), `pthread` | Multi-threaded HTTP web server, routing, static asset delivery, and JSON REST API |
| **Serialization** | `cJSON` (Dave Gamble) | Standard lightweight C JSON encoding and decoding |
| **Persistence** | C File Handling (`stdio.h`) | Flat-file record persistence (`people.dat`, `relationships.dat`) |
| **Frontend** | HTML5, CSS3, Vanilla JavaScript | Reactive UI, interactive SVG tree rendering, mouse pan/zoom, fetch API |
| **Typography** | Plus Jakarta Sans, JetBrains Mono | Modern SaaS aesthetics and clean code/metrics readability |

---

## 5. Data Structures Implemented Manually in C

### A. Person Structure (`backend/person.h`, `backend/person.c`)
Represents an individual with identity, biological, and historical metadata:
```c
typedef struct Person {
    char id[64];           // Unique key (e.g. "P01")
    char name[128];        // Full name
    char gender[16];       // "Male", "Female", "Other"
    int birth_year;        // e.g. 1940
    int death_year;        // 0 if living or unknown
    int is_living;         // 1 = living, 0 = deceased
    char bio[256];         // Notes, profession, achievements
    int generation;        // Dynamically computed generation rank
} Person;
```

### B. Graph Using Adjacency Lists (`backend/graph.h`, `backend/graph.c`)
The primary data structure modeling the family network.
- **Vertices (`GraphVertex`)**: Each individual is allocated exactly once.
- **Directed Parent-Child Edges (`Edge* children`, `Edge* parents`)**: Stored as singly linked lists. Maintaining both forward (child) and reverse (parent) edges enables $O(\text{deg})$ traversal in either direction without $O(V+E)$ graph scans.
- **Spouse Adjacency List (`Edge* spouses`)**: Decoupled from parent-child edges to cleanly model horizontal partnerships and multiple marriages.

### C. Hash Table with Separate Chaining (`backend/hashmap.h`, `backend/hashmap.c`)
- **Key**: String ID (e.g. `"P08"`).
- **Value**: Pointer to `GraphVertex`.
- **Hash Function**: DJB2 algorithm ($hash = ((hash \ll 5) + hash) + c$).
- **Collision Resolution**: Linked list separate chaining at each bucket.
- **Telemetry**: Exposes collision count, max chain length, load factor, and size.

### D. Dynamic FIFO Queue (`backend/queue.h`, `backend/queue.c`)
- Linked list based First-In-First-Out queue with $O(1)$ `queue_enqueue` and `queue_dequeue`.
- Drives Breadth-First Search (BFS) for generation partitioning, level-by-level descendant listing, and shortest kinship pathfinding.

### E. Dynamic LIFO Stack (`backend/stack.h`, `backend/stack.c`)
- Linked list based Last-In-First-Out stack with $O(1)$ `stack_push` and `stack_pop`.
- Powers iterative Depth-First Search (DFS) for ancestry lineage tracing and cycle detection without risking recursion stack overflow.

---

## 6. Algorithms Implemented

| Algorithm | Data Structure Used | Purpose |
|---|---|---|
| **BFS Generation Partitioning** | `Queue`, `HashMap` | Calculates generation numbers ($1, 2, 3\dots$) starting from root nodes ($\text{in\_degree} = 0$). Propagates $\text{gen} + 1$ to children and synchronizes spouses. |
| **Iterative DFS Ancestry Tracing** | `Stack`, `HashMap` | Climbs parent edges to discover all biological progenitors with distance metrics and DSA step-by-step trace logging. |
| **BFS Level-by-Level Descendant Exploration** | `Queue`, `HashMap` | Explores children, grandchildren, and great-grandchildren level by level. |
| **Ancestry Cycle Detection** | `Stack` (Iterative Reachability) | Before creating an edge `parent -> child`, verifies that `parent` is not reachable from `child` along existing child edges. Prevents temporal paradoxes. |
| **Shortest Kinship Pathfinding** | `Queue`, `HashMap` | Unweighted BFS across parent, child, and spouse edges to find the shortest connecting sequence between any two people. |
| **Lowest Common Ancestor (LCA)** | Set Intersection via `HashMap` | Computes the intersection of ancestor sets for two individuals, identifying shared elders and kinship degree. |
| **Kinship Classification** | Rule Engine | Automatically classifies: Self, Spouse, Parent, Child, Sibling (Full & Half), Grandparent, Grandchild, Aunt/Uncle, Niece/Nephew, First/Second Cousin, and In-laws. |
| **Cascade Deletion & Cleanup** | Pointer Re-linking | When a vertex is deleted, removes all incoming/outgoing parent edges, spouse links, removes from hash table, and frees memory cleanly. |

---

## 7. Project Architecture

```
KinGraph/
├── Makefile                     # Build automation (compile, test, run, clean)
├── README.md                    # Comprehensive engineering documentation
├── backend/
│   ├── main.c                  # Server initialization, CLI args, signals
│   ├── person.h / person.c     # Person entity allocation, clone, validation
│   ├── hashmap.h / hashmap.c   # DJB2 Chained Hash Table
│   ├── queue.h / queue.c       # FIFO Queue
│   ├── stack.h / stack.c       # LIFO Stack
│   ├── graph.h / graph.c       # Adjacency List Graph & Vertex management
│   ├── algorithms.h / .c       # BFS, DFS, LCA, Pathfinding, Cycle check
│   ├── storage.h / storage.c   # Flat-file persistence (people.dat, relationships.dat)
│   ├── server.h / server.c     # Multi-threaded POSIX socket HTTP server
│   ├── cJSON.h / cJSON.c       # Lightweight C JSON library
│   ├── test_dsa.c              # 65-test automated unit & integration test suite
│   └── data/
│       ├── people.dat          # Persistent flat-file storage for people
│       └── relationships.dat   # Persistent flat-file storage for edges
└── frontend/
    ├── index.html              # Modern SaaS UI (Hero, Dashboard, Tree, Explorer, DSA Lab)
    ├── style.css               # Dark theme design system, SVG node cards, transitions
    └── script.js               # Reactive DOM, SVG layout, zoom/pan, fetch() client
```

### HTTP REST API Endpoints

| Endpoint | Method | Description |
|---|---|---|
| `GET /api/stats` | GET | Tree statistics, edge counts, and live C hash table metrics |
| `GET /api/family` | GET | Complete family graph (people, parent-child edges, spouse edges) |
| `GET /api/person?id=...` | GET | Detailed record of a person (parents, children, spouse) |
| `POST /api/person` | POST | Adds a new person with input validation |
| `PUT /api/person` | PUT | Edits an existing member |
| `DELETE /api/person?id=...` | DELETE | Deletes a member and cascades edge removal |
| `POST /api/relationship` | POST | Connects parent-child (with cycle check) or spouse |
| `DELETE /api/relationship` | DELETE | Disconnects a relationship |
| `GET /api/ancestors?id=...` | GET | Ancestor list and iterative DFS execution trace |
| `GET /api/descendants?id=...` | GET | Descendant list and BFS queue execution trace |
| `GET /api/relationship?a=&b=`| GET | Relationship classification, LCA, and shortest path |
| `GET /api/generations` | GET | BFS generation-by-generation breakdown |
| `POST /api/save` | POST | Flushes in-memory graph to disk |
| `POST /api/load` | POST | Reloads saved records from disk |
| `POST /api/reset` | POST | Restores the 15-person 4-generation sample dynasty |

---

## 8. How to Compile and Run

### Prerequisites
- macOS or Linux
- Clang or GCC compiler with `pthread` support
- GNU Make

### Build & Run Commands

```bash
# 1. Clone or navigate to the repository
cd KinGraph

# 2. Run the automated test suite (verifies all 65 DSA test cases)
make test

# 3. Compile the application
make

# 4. Start the C Backend Server
./kingraph_server --port 8080 --web-root frontend
# or using Makefile:
make run

# 5. Open your web browser
open http://localhost:8080
```

---

## 9. Time and Space Complexity Analysis

| Operation / Feature | Data Structure / Algorithm | Average Time Complexity | Worst-Case Time | Space Complexity |
|---|---|---|---|---|
| **Member Lookup by ID** | Chained Hash Table (`DJB2`) | $O(1)$ | $O(N)$ (rare collision) | $O(V)$ |
| **Add Member** | Hash Table + Vertex list | $O(1)$ | $O(N)$ | $O(1)$ |
| **Connect Relationship** | Reachability DFS + Edge List | $O(V + E)$ | $O(V + E)$ | $O(V)$ |
| **Delete Member** | Hash Map + Edge Removal | $O(\text{deg})$ | $O(V + E)$ | $O(1)$ |
| **Compute Generations** | BFS using Dynamic Queue | $O(V + E)$ | $O(V + E)$ | $O(V)$ |
| **Ancestor Retrieval** | Iterative DFS using Dynamic Stack | $O(V_{\text{anc}} + E_{\text{anc}})$ | $O(V + E)$ | $O(V)$ |
| **Descendant Retrieval** | BFS using Dynamic Queue | $O(V_{\text{desc}} + E_{\text{desc}})$ | $O(V + E)$ | $O(V)$ |
| **Shortest Kinship Path**| BFS using Dynamic Queue | $O(V + E)$ | $O(V + E)$ | $O(V)$ |
| **Lowest Common Ancestor**| Dual DFS + Hash Intersection | $O(V_{\text{anc}})$ | $O(V + E)$ | $O(V)$ |
| **Overall Graph Storage**| Adjacency Lists | — | — | $O(V + E)$ |

---

## 10. Sample Operations & Walkthrough

### Querying Kinship Between First Cousins
```bash
curl -s "http://localhost:8080/api/relationship?a=P08&b=P10"
```
**JSON Response:**
```json
{
  "person_a_id": "P08",
  "person_a_name": "Lucas Sterling",
  "person_b_id": "P10",
  "person_b_name": "Sophia Thorne",
  "relationship": "First Cousin",
  "degree": 4,
  "common_ancestors": [
    { "id": "P01", "name": "Arthur Sterling", "total_distance": 4 },
    { "id": "P02", "name": "Eleanor Vance", "total_distance": 4 }
  ],
  "path": [
    { "node_id": "P08", "node_name": "Lucas Sterling", "edge_type": "" },
    { "node_id": "P03", "node_name": "Julian Sterling", "edge_type": "child_of" },
    { "node_id": "P01", "node_name": "Arthur Sterling", "edge_type": "child_of" },
    { "node_id": "P05", "node_name": "Beatrice Sterling", "edge_type": "parent_of" },
    { "node_id": "P10", "node_name": "Sophia Thorne", "edge_type": "parent_of" }
  ],
  "success": true
}
```

### Attempting an Ancestry Cycle
```bash
# Trying to make Grandparent (P01) the child of Grandchild (P08):
curl -s -X POST http://localhost:8080/api/relationship \
  -H "Content-Type: application/json" \
  -d '{"type":"parent_child","parent_id":"P08","child_id":"P01"}'
```
**Rejection Verdict:**
```json
{
  "success": false,
  "error": "Cycle detected: 'P08' is already a descendant of 'P01'. Adding this edge would create an ancestral loop."
}
```

---

## 11. Implemented Features

- [x] **Person Management**: Full lifecycle (create, validate, clone, edit, delete, JSON serialization).
- [x] **Adjacency List Graph**: Primary data structure modeling vertices and directed parent-child edges.
- [x] **Separate Spouse Connections**: Bidirectional orthogonal marriage edges.
- [x] **Chained Hash Table**: $O(1)$ lookups by string ID with collision tracking and live telemetry.
- [x] **Dynamic FIFO Queue**: Custom linked list queue for BFS.
- [x] **Dynamic LIFO Stack**: Custom linked list stack for iterative DFS.
- [x] **BFS Generation Breakdown**: Level-by-level generation partitioning.
- [x] **Iterative DFS Ancestor Retrieval**: Stack-based upward exploration with execution trace logging.
- [x] **BFS Descendant Retrieval**: Queue-based downward exploration with execution trace logging.
- [x] **Cycle Detection Engine**: Prohibits generational paradoxes through iterative reachability checks.
- [x] **Kinship Classifier & Shortest Path**: Classifies 12 relationship categories with step-by-step breadcrumb paths.
- [x] **Common Ancestor & LCA Identification**: Identifies shared ancestors and kinship distance.
- [x] **Persistent C Storage**: Flat-file persistence in `people.dat` and `relationships.dat`.
- [x] **POSIX Sockets HTTP Server**: Serves frontend assets and JSON REST API with thread safety (`pthread_mutex`).
- [x] **Interactive SVG Family Tree Studio**: Smooth mouse pan, scroll zoom, card rendering, and color-coded ancestry/descendant glow.
- [x] **Modern SaaS UI**: Responsive dark theme, hero section, dashboard telemetry, relationship explorer, and DSA console.
- [x] **Automated Test Suite**: 65 unit and integration tests covering all DSA components.

---

## 12. Partially Implemented & Future Enhancements

- **Consanguinity Coefficient Calculation**: Currently calculates shortest path degree and Lowest Common Ancestor. Wright's Coefficient of Relationship calculation for complex inbreeding coefficients can be added.
- **GEDCOM File Format Import/Export**: Currently uses custom pipe-delimited flat-file persistence. Full standard GEDCOM 5.5 parsing is planned.
- **DNA Match Percentage Simulation**: Synthesizing expected autosomal centiMorgan (cM) inheritance distributions across paths.
