#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define NUM_CHILDREN 3
#define BUFFER_SIZE 20

int pipes_to_child[NUM_CHILDREN][2];   // Pipes for sending data to children
int pipes_to_parent[NUM_CHILDREN][2];  // Pipes for receiving results from children
int child_index;  // Global variable to identify child process index

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

    // Fork child processes and set up parent or child process based on fork result
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
        wait(NULL);
    }

    return 0;
}

void setup_child(int index) {
    child_index = index;

    // Set up signal handler
    if (index == 0) {
        signal(SIGUSR1, handle_signal);  // Addition
    } else if (index == 1) {
        signal(SIGUSR2, handle_signal);  // Subtraction
    } else if (index == 2) {
        signal(SIGALRM, handle_signal);  // Multiplication
    }

    // Close unused pipe ends
    close(pipes_to_child[index][1]);    // Close write end to child
    close(pipes_to_parent[index][0]);   // Close read end from child

    // Wait for signal and perform calculation
    while (1) {
        pause();  // Wait for signal
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
        result = nums[0] + nums[1];  // Addition
    } else if (signum == SIGUSR2) {
        result = nums[0] - nums[1];  // Subtraction
    } else if (signum == SIGALRM) {
        result = nums[0] * nums[1];  // Multiplication
    } else {
        fprintf(stderr, "Child: Received unknown signal\n");
        exit(EXIT_FAILURE);
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
        close(pipes_to_child[i][0]);    // Close read end to child
        close(pipes_to_parent[i][1]);   // Close write end from child
    }

    char input[BUFFER_SIZE];
    int num1, num2;
    char op;

    while (1) {
        printf("Enter two integers and an operation (+, -, *) or 'q' to quit: ");
        fgets(input, BUFFER_SIZE, stdin);

        if (input[0] == 'q') {
            break;
        }

        if (sscanf(input, "%d %d %c", &num1, &num2, &op) != 3) {
            printf("Invalid input. Please try again.\n");
            continue;
        }

        int index;
        int signum;

        // Determine which child process to use based on the operation
        if (op == '+') {
            index = 0;
            signum = SIGUSR1;
        } else if (op == '-') {
            index = 1;
            signum = SIGUSR2;
        } else if (op == '*') {
            index = 2;
            signum = SIGALRM;
        } else {
            printf("Invalid operation. Please use +, -, or *.\n");
            continue;
        }

        // Send data to the appropriate child process
        int nums[2] = {num1, num2};
        if (write(pipes_to_child[index][1], nums, sizeof(nums)) != sizeof(nums)) {
            perror("Parent: Error writing numbers to child");
            continue;
        }

        // Signal child to perform calculation
        printf("The child process with PID: %d will provide the result.\n", child_pids[index]);
        if (kill(child_pids[index], signum) == -1) {
            perror("Parent: Error sending signal to child");
            continue;
        }

        // Read result from child
        int result;
        if (read(pipes_to_parent[index][0], &result, sizeof(result)) != sizeof(result)) {
            perror("Parent: Error reading result from child");
            continue;
        }

        // Display result
        printf("Result: %d\n\n", result);
    }

    // Close pipes
    for (int i = 0; i < NUM_CHILDREN; i++) {
        close(pipes_to_child[i][1]);
        close(pipes_to_parent[i][0]);
    }

    // Terminate child processes
    for (int i = 0; i < NUM_CHILDREN; i++) {
        kill(child_pids[i], SIGTERM);
    }
}

