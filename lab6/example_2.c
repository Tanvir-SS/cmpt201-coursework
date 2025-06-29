#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

/*–– Macros ––*/
#define ASSERT(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr,                                                          \
              "Assertion failed: %s\n"                                         \
              "  File: %s, Line: %d\n",                                        \
              #expr, __FILE__, __LINE__);                                      \
      exit(EXIT_FAILURE);                                                      \
    }                                                                          \
  } while (0)

#define TEST(expr)                                                             \
  do {                                                                         \
    if (!(expr)) {                                                             \
      fprintf(stderr, "Test failed: %s\n", #expr);                             \
      exit(EXIT_FAILURE);                                                      \
    } else {                                                                   \
      printf("Test passed: %s\n", #expr);                                      \
    }                                                                          \
  } while (0)

/*–– Linked-list ADT ––*/
typedef struct node {
  uint64_t data;
  struct node *next;
} node_t;

typedef struct {
  node_t *head;
  uint64_t sum;
} list_t;

/* Initialize an empty list */
static void list_init(list_t *lst) {
  lst->head = NULL;
  lst->sum = 0;
}

/* Insert `value` into the list in ascending order */
static void list_insert_sorted(list_t *lst, uint64_t value) {
  node_t *new_node = malloc(sizeof *new_node);
  ASSERT(new_node != NULL);
  new_node->data = value;
  new_node->next = NULL;

  /* Find insertion point (pointer-to-pointer trick) */
  node_t **pp = &lst->head;
  while (*pp && (*pp)->data < value) {
    pp = &(*pp)->next;
  }
  new_node->next = *pp;
  *pp = new_node;

  lst->sum += value;
}

/* Return zero-based index of `value`, or –1 if not found */
static int list_index_of(const list_t *lst, uint64_t value) {
  node_t *cur = lst->head;
  int idx = 0;
  while (cur) {
    if (cur->data == value)
      return idx;
    cur = cur->next;
    idx++;
  }
  return -1;
}

/* Walk the list and recompute the sum */
static uint64_t list_calculate_sum(const list_t *lst) {
  uint64_t s = 0;
  node_t *cur = lst->head;
  while (cur) {
    s += cur->data;
    cur = cur->next;
  }
  return s;
}

/* Free all nodes and reset the list */
static void list_destroy(list_t *lst) {
  node_t *cur = lst->head;
  while (cur) {
    node_t *tmp = cur->next;
    free(cur);
    cur = tmp;
  }
  lst->head = NULL;
  lst->sum = 0;
}

/*–– Example & Tests ––*/
int main(void) {
  list_t lst;
  list_init(&lst);

  list_insert_sorted(&lst, 1);
  list_insert_sorted(&lst, 3);
  list_insert_sorted(&lst, 5);
  list_insert_sorted(&lst, 2);

  /* List is now [1,2,3,5] */
  TEST(list_index_of(&lst, 2) == 1);
  TEST(lst.sum == 1 + 3 + 5 + 2);
  ASSERT(lst.sum == list_calculate_sum(&lst));

  /* More inserts */
  list_insert_sorted(&lst, 0);
  list_insert_sorted(&lst, 4);

  /* List is now [0,1,2,3,4,5] */
  TEST(list_index_of(&lst, 0) == 0);
  TEST(list_index_of(&lst, 4) == 4);
  ASSERT(lst.sum == 0 + 1 + 2 + 3 + 4 + 5);
  ASSERT(lst.sum == list_calculate_sum(&lst));

  list_destroy(&lst);
  return 0;
}
