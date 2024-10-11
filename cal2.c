#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

#define NUM_CHILDREN 3
#define BUFFER_SIZE 20

int pipes_to_child[NUM_CHILDREN][2];    // Pipes for sending data to children
int pipes_to_parent[NUM_CHILDREN][2];   // Pipes for receiving results from children
int child_index;                        // Child process index

void setup_child(int index);
void parent_process(pid_t child_pids[]);
void handle_signal(int signum);

int main() {
    pid_t child_pids[NUM_CHILDREN];

    // Create pipes for each child
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pipe(pipes_to_child[i]) == -1) {
            perror("Error creating pipe to child");
            exit(EXIT_FAILURE);
        }
        if (pipe(pipes_to_parent[i]) == -1) {
            perror("Error creating pipe to parent");
            exit(EXIT_FAILURE);
        }
    }

    // Fork child processes
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("Error forking child process");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Child process
            setup_child(i);
            exit(0);
        } else {
            // Parent process
            child_pids[i] = pid;
        }
    }

    // Parent process handles user input and communication
    parent_process(child_pids);

    // Wait for child processes to finish
    for (int i = 0; i < NUM_CHILDREN; i++) {
        waitpid(child_pids[i], NULL, 0);
    }

    return 0;
}

void setup_child(int index) {
    child_index = index;

    // Close unused pipe ends
    close(pipes_to_child[index][1]);    // Close write end of pipe to child
    close(pipes_to_parent[index][0]);   // Close read end of pipe to parent

    // Set up signal handler
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    int signum;
    if (index == 0) {
        signum = SIGUSR1; // Addition
    } else if (index == 1) {
        signum = SIGUSR2; // Subtraction
    } else if (index == 2) {
        signum = SIGCHLD; // Multiplication
    }

    if (sigaction(signum, &sa, NULL) == -1) {
        perror("Error setting up signal handler");
        exit(EXIT_FAILURE);
    }

    // Wait for signals indefinitely
    while (1) {
        pause();
    }
}

void handle_signal(int signum) {
    int nums[2];

    // Read two integers from the parent
    if (read(pipes_to_child[child_index][0], nums, sizeof(nums)) != sizeof(nums)) {
        perror("Child: Error reading numbers");
        exit(EXIT_FAILURE);
    }

    int result;
    // Perform the calculation based on the signal received
    if (signum == SIGUSR1) {
        result = nums[0] + nums[1];
    } else if (signum == SIGUSR2) {
        result = nums[0] - nums[1];
    } else if (signum == SIGCHLD) {
        result = nums[0] * nums[1];
    } else {
        fprintf(stderr, "Child: Received unknown signal\n");
        return;
    }

    // Send the result back to the parent
    if (write(pipes_to_parent[child_index][1], &result, sizeof(result)) != sizeof(result)) {
        perror("Child: Error writing result");
        exit(EXIT_FAILURE);
    }
}

void parent_process(pid_t child_pids[]) {
    // Close unused pipe ends
    for (int i = 0; i < NUM_CHILDREN; i++) {
        close(pipes_to_child[i][0]);    // Parent doesn't read from pipes_to_child
        close(pipes_to_parent[i][1]);   // Parent doesn't write to pipes_to_parent
    }

    char input[BUFFER_SIZE];
    while (1) {
        printf("Enter two integers and an operation (+, -, *) or 'q' to quit: ");
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }

        if (input[0] == 'q') {
            break;
        }

        int num1, num2;
        char op;
        if (sscanf(input, "%d %d %c", &num1, &num2, &op) != 3) {
            printf("Invalid input. Please try again.\n\n");
            continue;
        }

        int index;
        int signum;
        if (op == '+') {
            index = 0;
            signum = SIGUSR1;
        } else if (op == '-') {
            index = 1;
            signum = SIGUSR2;
        } else if (op == '*') {
            index = 2;
            signum = SIGCHLD;
        } else {
            printf("Invalid operation. Please use +, -, or *.\n\n");
            continue;
        }

        // Send data to the appropriate child process
        int nums[2] = {num1, num2};
        if (write(pipes_to_child[index][1], nums, sizeof(nums)) != sizeof(nums)) {
            perror("Parent: Error writing numbers to child");
            continue;
        }

        // Signal the child to perform the calculation
        printf("The child process with PID: %d will provide the result.\n", child_pids[index]);
        if (kill(child_pids[index], signum) == -1) {
            perror("Parent: Error sending signal to child");
            continue;
        }

        // Read the result from the child
        int result;
        if (read(pipes_to_parent[index][0], &result, sizeof(result)) != sizeof(result)) {
            perror("Parent: Error reading result from child");
            continue;
        }

        // Display the result
        printf("Result: %d\n\n", result);
    }

    // Terminate child processes
    for (int i = 0; i < NUM_CHILDREN; i++) {
        kill(child_pids[i], SIGTERM);
    }
}
