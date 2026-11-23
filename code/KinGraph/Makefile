CC = clang
CFLAGS = -Wall -Wextra -O2 -pthread -Ibackend
SRCS = backend/person.c backend/hashmap.c backend/queue.c backend/stack.c \
       backend/graph.c backend/algorithms.c backend/storage.c backend/cJSON.c

OBJS = $(SRCS:.c=.o)

SERVER_BIN = kingraph_server
TEST_BIN = kingraph_test

all: $(SERVER_BIN) $(TEST_BIN)

$(SERVER_BIN): $(OBJS) backend/server.o backend/main.o
	$(CC) $(CFLAGS) -o $@ $^

$(TEST_BIN): $(OBJS) backend/test_dsa.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

run: $(SERVER_BIN)
	./$(SERVER_BIN) --port 8080 --web-root frontend

clean:
	rm -f backend/*.o $(SERVER_BIN) $(TEST_BIN) /tmp/test_kingraph_*.dat

.PHONY: all test run clean
