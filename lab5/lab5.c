#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct header {
  uint64_t size;
  struct header *next;
  int id;
};

void initialize_block(struct header *block, uint64_t size, struct header *next,
                      int id) {
  block->size = size;
  block->next = next;
  block->id = id;
}

int find_first_fit(struct header *free_list_ptr, uint64_t size) {
  // TODO: Implement first fit
  struct header *temp = free_list_ptr;
  while (temp != NULL) {
    if (temp->size >= size) {
      return temp->id;
    } else {
      temp = temp->next;
    }
  }
  return -1;
}

int find_best_fit(struct header *free_list_ptr, uint64_t size) {

  struct header *cur = free_list_ptr;
  uint64_t best_size = UINT64_MAX;
  int best_id = -1;

  while (cur) {
    if (cur->size >= size && cur->size < best_size) {
      best_size = cur->size;
      best_id = cur->id;
    }
    cur = cur->next;
  }
  return best_id;
}

int find_worst_fit(struct header *free_list_ptr, uint64_t size) {
  struct header *cur = free_list_ptr;
  uint64_t worst_size = 0;
  int worst_id = -1;

  while (cur) {
    if (cur->size >= size && cur->size > worst_size) {
      worst_size = cur->size;
      worst_id = cur->id;
    }
    cur = cur->next;
  }
  return worst_id;
}

int main(void) {

  struct header *free_block1 = (struct header *)malloc(sizeof(struct header));
  struct header *free_block2 = (struct header *)malloc(sizeof(struct header));
  struct header *free_block3 = (struct header *)malloc(sizeof(struct header));
  struct header *free_block4 = (struct header *)malloc(sizeof(struct header));
  struct header *free_block5 = (struct header *)malloc(sizeof(struct header));

  initialize_block(free_block1, 6, free_block2, 1);
  initialize_block(free_block2, 12, free_block3, 2);
  initialize_block(free_block3, 24, free_block4, 3);
  initialize_block(free_block4, 8, free_block5, 4);
  initialize_block(free_block5, 4, NULL, 5);

  struct header *free_list_ptr = free_block1;

  int first_fit_id = find_first_fit(free_list_ptr, 7);
  int best_fit_id = find_best_fit(free_list_ptr, 7);
  int worst_fit_id = find_worst_fit(free_list_ptr, 7);

  printf("The ID for First-Fit algorithm is: %d\n", first_fit_id);
  printf("The ID for Best-Fit algorithm is: %d\n", best_fit_id);
  printf("The ID for Worst-Fit algorithm is: %d\n", worst_fit_id);

  return 0;
}
/*
 * Pseudocode for coalescing a newly freed block:
 *
 * function coalesce_new_block(new_block, free_list):
 *
 *   // Step 1: Insert new_block into free_list in sorted order by address.
 *   prev = NULL
 *   curr = free_list
 *   while curr is not NULL and (address of curr < address of new_block):
 *       prev = curr
 *       curr = curr.next
 *   new_block.next = curr
 *   if prev is NULL:
 *       free_list = new_block
 *   else:
 *       prev.next = new_block
 *
 *   // Step 2: Merge with the block immediately before (if adjacent).
 *   if prev is not NULL and (address of prev + sizeof(header) + prev.size ==
 * address of new_block):
 *       // Merge by updating the previous block's size.
 *       prev.size = prev.size + sizeof(header) + new_block.size
 *       new_block = prev
 *
 *   // Step 3: Merge with the block immediately after (if adjacent).
 *   if curr is not NULL and (address of new_block + sizeof(header) +
 * new_block.size == address of curr):
 *       // Merge by updating the new_block's size.
 *       new_block.size = new_block.size + sizeof(header) + curr.size
 *       new_block.next = curr.next
 *
 *   return free_list
 */
