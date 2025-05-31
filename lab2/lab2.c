// Lab2
// Date: May 30 2025
// Author: Tanvir Shergill
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  char *input = NULL;
  size_t len = 0;

  printf("Enter program to run.\n");

  while (true) {
    int wstatus = 0;

    printf("> ");
    if (getline(&input, &len, stdin) == -1) {
      perror("getline failed");
      break;
    }

    // Remove newline
    input[strcspn(input, "\n")] = '\0';

    char *token = NULL;
    char *saveptr = NULL;

    token = strtok_r(input, " ", &saveptr);

    pid_t child = fork();
    if (child == 0) {
      // Child process

      execl(token, token, NULL);
      perror("Exec failure");
      exit(EXIT_FAILURE);
    }

    if (waitpid(child, &wstatus, 0) == -1) {
      perror("child failed");
      exit(EXIT_FAILURE);
    } else {
      if (WIFEXITED(wstatus)) {
        printf("Enter program to run.\n");
      }
    }
  }

  free(input);
  return 0;
}
