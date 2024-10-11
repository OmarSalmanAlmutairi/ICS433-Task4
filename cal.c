#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

#define NUM_CHILDREN 3
#define BUFFER_SIZE 20

int pipes_to_child[NUM_CHILDREN][2]; // Pipes for sending data to children
int pipes_to_parent[NUM_CHILDREN][2]; // Pipes for receiving results from children

void setup_child(int index);
void parent_process(pid_t child_pids[]);
void handle_signal(int signum);

volatile sig_atomic_t operation_ready = 0;

int main() {
    pid_t child_pids[NUM_CHILDREN];

    // Create pipes for each child
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (pipe(pipes_to_child[i]) == -1 || pipe(pipes_to_parent[i]) == -1) {
            perror("Pipe creation failed");
            exit(EXIT_FAILURE);
        }
    }

    // Fork child processes
    for (int i = 0; i < NUM_CHILDREN; i++) {
        child_pids[i] = fork();
        if (child_pids[i] == -1) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        } else if (child_pids[i] == 0) {
            setup_child(i);
            exit(EXIT_SUCCESS);
        }
    }

    // Parent process
    parent_process(child_pids);

    // Wait for all child processes to finish
    for (int i = 0; i < NUM_CHILDREN; i++) {
        wait(NULL);
    }

    return 0;
}

void setup_child(int index) {
    // Set up signal handler
    signal(SIGUSR1, handle_signal);
    signal(SIGUSR2, handle_signal);
    signal(SIGCHLD, handle_signal);

    // Close unused pipe ends
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (i != index) {
            close(pipes_to_child[i][0]);
            close(pipes_to_parent[i][1]);
        }
        close(pipes_to_child[i][1]);
        close(pipes_to_parent[i][0]);
    }

    int num1, num2, result;
    while (1) {
        // Wait for signal
        while (!operation_ready) {
            pause();
        }
        operation_ready = 0;

        // Read data from pipe
        read(pipes_to_child[index][0], &num1, sizeof(int));
        read(pipes_to_child[index][0], &num2, sizeof(int));

        // Perform calculation
        switch (index) {
            case 0: result = num1 + num2; break;
            case 1: result = num1 - num2; break;
            case 2: result = num1 * num2; break;
        }

        // Send result back to parent
        write(pipes_to_parent[index][1], &result, sizeof(int));
    }
}

void parent_process(pid_t child_pids[]) {
    // Close unused pipe ends
    for (int i = 0; i < NUM_CHILDREN; i++) {
        close(pipes_to_child[i][0]);
        close(pipes_to_parent[i][1]);
    }

    char input[BUFFER_SIZE];
    int num1, num2, result;
    char operation;

    while (1) {
        printf("Enter two integers and an operation (+, -, *) or 'q' to quit: ");
        if (fgets(input, BUFFER_SIZE, stdin) == NULL) {
            break;
        }

        if (input[0] == 'q') {
            break;
        }

        if (sscanf(input, "%d %d %c", &num1, &num2, &operation) != 3) {
            printf("Invalid input. Please try again.\n");
            continue;
        }

        int child_index;
        switch (operation) {
            case '+': child_index = 0; break;
            case '-': child_index = 1; break;
            case '*': child_index = 2; break;
            default:
                printf("Invalid operation. Please use +, -, or *.\n");
                continue;
        }

        // Send data to appropriate child process
        write(pipes_to_child[child_index][1], &num1, sizeof(int));
        write(pipes_to_child[child_index][1], &num2, sizeof(int));

        // Signal child to perform calculation
        switch (child_index) {
            case 0: kill(child_pids[0], SIGUSR1); break;
            case 1: kill(child_pids[1], SIGUSR2); break;
            case 2: kill(child_pids[2], SIGCHLD); break;
        }

        // Read result from child
        read(pipes_to_parent[child_index][0], &result, sizeof(int));

        // Display result
        printf("The child process with PID: %d will provide the result.\n", child_pids[child_index]);
        printf("Result: %d\n\n", result);
    }

    // Send termination signal to all child processes
    for (int i = 0; i < NUM_CHILDREN; i++) {
        kill(child_pids[i], SIGTERM);
    }
}

void handle_signal(int signum) {
    operation_ready = 1;
}
