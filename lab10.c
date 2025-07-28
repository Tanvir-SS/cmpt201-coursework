/*
Understanding the Client:
1. How is the client sending data to the server? What protocol?
The client uses TCP to send data to server through the stream socket over IPv4

2. What data is the client sending to the server?
The data the client is sending consists of five text messages ("Hello", "Apple",
"Car", "Green", "Dog"), which are padded to 1024 bytes using strncpy and sent
using write().

Understanding the Server:
1.Explain the argument that the `run_acceptor` thread is passed as an argument.
the run_acceptor thread, we bundle all of its inputs into a single acceptor_args
struct and pass a pointer to that struct as the thread’s argument.
Inside run_acceptor, we cast the void* back to acceptor_args* and extract:

2. How are received messages stored?
Received messages are stored in a dynamically allocated list_node on the heap.
Each node contains a 1024-byte buffer that holds the incoming message and a
pointer to the next node in the list. When a message arrives, the server
allocates a new node, copies the data into its buffer, and appends it to the
tail of the shared linked list inside a critical section protected by a mutex to
ensure thread safety.

3. What does `main()` do with the received messages?
In main(), the program waits until the expected number of messages have been
enqueued by the client threads. Once that threshold is reached, main() signals
the acceptor and client threads to stop and then joins them to ensure they
terminate cleanly. It then calls collect_all(), which traverses the linked list,
prints each stored message, and frees its memory. Finally, main() verifies that
the total message count matches expectations and destroys the mutex before
performing any remaining cleanup.

4. How are threads used in this sample code?
The sample code employs a multithreaded design to handle clients concurrently. A
dedicated acceptor thread listens for incoming connections and spawns a new
client handler thread for each accepted socket. Each client thread reads
messages from its connection and enqueues them into the shared message list,
using a mutex to synchronize insertions and prevent data races. This structure
allows the server to manage multiple clients in parallel while preserving the
integrity of shared data.
*/

/*
1. How are sockets made non-blocking?
fcntl() makes sockets non-bloakcing by setting the O_NONBLOCK flag:

2. What sockets are made non-blocking?
The code configures both the server’s listening socket, which is initialized in
init_server_socket(), and each client connection socket created in
run_client() to operate in non-blocking mode.

3. Why are these sockets made non-blocking? What purpose does it serve?
Enabling non-blocking mode prevents socket operations from suspending thread
execution. This allows the acceptor thread to poll for new client connections
without hanging when none are available. Likewise, each client handler
thread can attempt to read incoming data and immediately continue if there is
nothing to read, ensuring the server remains responsive,
can recheck its control flags, and handle multiple clients concurrently without
deadlocks or stalls.

*/

/*******server.c**********/

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define PORT 8001
#define LISTEN_BACKLOG 32
#define MAX_CLIENTS 4
#define NUM_MSG_PER_CLIENT 5

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

struct list_node {
  struct list_node *next;
  void *data;
};

struct list_handle {
  struct list_node *last;
  volatile uint32_t count;
};

struct client_args {
  atomic_bool run;
  int cfd;
  struct list_handle *list_handle;
  pthread_mutex_t *list_lock;
};

struct acceptor_args {
  atomic_bool run;
  struct list_handle *list_handle;
  pthread_mutex_t *list_lock;
};

int init_server_socket() {
  struct sockaddr_in addr;

  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1) {
    handle_error("bind");
  }

  if (listen(sfd, LISTEN_BACKLOG) == -1) {
    handle_error("listen");
  }

  return sfd;
}

void set_non_blocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    perror("fcntl F_GETFL");
    exit(EXIT_FAILURE);
  }
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("fcntl F_SETFL");
    exit(EXIT_FAILURE);
  }
}

void add_to_list(struct list_handle *list_handle, struct list_node *new_node) {
  struct list_node *last_node = list_handle->last;
  last_node->next = new_node;
  list_handle->last = last_node->next;
  list_handle->count++;
}

int collect_all(struct list_node head) {
  struct list_node *node = head.next;
  uint32_t total = 0;

  while (node != NULL) {
    printf("Collected: %s\n", (char *)node->data);
    total++;

    struct list_node *next = node->next;
    free(node->data);
    free(node);
    node = next;
  }

  return total;
}

static void *run_client(void *args) {
  struct client_args *cargs = (struct client_args *)args;
  int cfd = cargs->cfd;
  set_non_blocking(cfd);

  char msg_buf[BUF_SIZE];

  while (cargs->run) {
    ssize_t bytes_read = read(cfd, &msg_buf, BUF_SIZE);
    if (bytes_read == -1) {
      if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
        perror("Problem reading from socket!\n");
        break;
      }
    } else if (bytes_read > 0) {
      struct list_node *new_node = malloc(sizeof(struct list_node));
      new_node->next = NULL;
      new_node->data = malloc(BUF_SIZE);
      memcpy(new_node->data, msg_buf, BUF_SIZE);

      struct list_handle *list_handle = cargs->list_handle;
      pthread_mutex_lock(cargs->list_lock);
      add_to_list(list_handle, new_node);
      pthread_mutex_unlock(cargs->list_lock);
    }
  }

  if (close(cfd) == -1) {
    perror("client thread close");
  }
  return NULL;
}

static void *run_acceptor(void *args) {
  int sfd = init_server_socket();
  set_non_blocking(sfd);

  struct acceptor_args *aargs = (struct acceptor_args *)args;
  pthread_t threads[MAX_CLIENTS];
  struct client_args client_args[MAX_CLIENTS];

  printf("Accepting clients...\n");

  uint16_t num_clients = 0;
  while (aargs->run) {
    if (num_clients < MAX_CLIENTS) {
      int cfd = accept(sfd, NULL, NULL);
      if (cfd == -1) {
        if (!(errno == EAGAIN || errno == EWOULDBLOCK)) {
          handle_error("accept");
        }
      } else {
        printf("Client connected!\n");

        client_args[num_clients].cfd = cfd;
        client_args[num_clients].run = true;
        client_args[num_clients].list_handle = aargs->list_handle;
        client_args[num_clients].list_lock = aargs->list_lock;

        if (pthread_create(&threads[num_clients], NULL, run_client,
                           &client_args[num_clients]) != 0) {
          perror("pthread_create for client");
          continue;
        }

        num_clients++;
      }
    }
  }

  printf("Not accepting any more clients!\n");

  for (int i = 0; i < num_clients; i++) {
    client_args[i].run = false;
    pthread_join(threads[i], NULL);
  }

  if (close(sfd) == -1) {
    perror("closing server socket");
  }
  return NULL;
}

int main() {
  pthread_mutex_t list_mutex;
  pthread_mutex_init(&list_mutex, NULL);

  struct list_node head = {NULL, NULL};
  struct list_node *last = &head;
  struct list_handle list_handle = {
      .last = &head,
      .count = 0,
  };

  pthread_t acceptor_thread;
  struct acceptor_args aargs = {
      .run = true,
      .list_handle = &list_handle,
      .list_lock = &list_mutex,
  };
  pthread_create(&acceptor_thread, NULL, run_acceptor, &aargs);

  // Wait until all messages are received
  while (list_handle.count < MAX_CLIENTS * NUM_MSG_PER_CLIENT) {
    usleep(100000); // 100ms
  }

  aargs.run = false;
  pthread_join(acceptor_thread, NULL);

  if (list_handle.count != MAX_CLIENTS * NUM_MSG_PER_CLIENT) {
    printf("Not enough messages were received!\n");
    return 1;
  }

  int collected = collect_all(head);
  printf("Collected: %d\n", collected);
  if (collected != list_handle.count) {
    printf("Not all messages were collected!\n");
    return 1;
  } else {
    printf("All messages were collected!\n");
  }

  pthread_mutex_destroy(&list_mutex);

  return 0;
}

/***********************client.c*********************/

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8001
#define BUF_SIZE 1024
#define ADDR "127.0.0.1"

#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define NUM_MSG 5

static const char *messages[NUM_MSG] = {"Hello", "Apple", "Car", "Green",
                                        "Dog"};

int main() {
  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1) {
    handle_error("socket");
  }

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(PORT);
  if (inet_pton(AF_INET, ADDR, &addr.sin_addr) <= 0) {
    handle_error("inet_pton");
  }

  if (connect(sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    handle_error("connect");
  }

  char buf[BUF_SIZE];
  for (int i = 0; i < NUM_MSG; i++) {
    sleep(1);
    // prepare message
    // this pads the desination with NULL
    strncpy(buf, messages[i], BUF_SIZE);

    if (write(sfd, buf, BUF_SIZE) == -1) {
      handle_error("write");
    } else {
      printf("Sent: %s\n", messages[i]);
    }
  }

  exit(EXIT_SUCCESS);
}
