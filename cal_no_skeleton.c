#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>

#define SIZE 50

int pfd_to_child[2]; // Pipe to send data to child
int pfd_to_parent[2]; // Pipe to receive data from child

void signal_handler(int signum);

int main() {
    pid_t pid_add, pid_sub, pid_mul;

    // Create pipes
    if (pipe(pfd_to_child) == -1) {
        perror("Error creating pipe to child");
        exit(EXIT_FAILURE);
    }
    if (pipe(pfd_to_parent) == -1) {
        perror("Error creating pipe to parent");
        exit(EXIT_FAILURE);
    }

    // Fork addition process
    pid_add = fork();
    if (pid_add == -1) {
        perror("Error forking addition process");
        exit(EXIT_FAILURE);
    } else if (pid_add == 0) {  // Addition child process
        signal(SIGUSR1, signal_handler);
        close(pfd_to_child[1]);  // Close write end of pipe to child
        close(pfd_to_parent[0]); // Close read end of pipe to parent
        pause();                 // Wait for signal
        exit(0);
    } else {
        // Fork subtraction process
        pid_sub = fork();
        if (pid_sub == -1) {
            perror("Error forking subtraction process");
            exit(EXIT_FAILURE);
        } else if (pid_sub == 0) {  // Subtraction child process
            signal(SIGUSR2, signal_handler);
            close(pfd_to_child[1]);  // Close write end of pipe to child
            close(pfd_to_parent[0]); // Close read end of pipe to parent
            pause();                 // Wait for signal
            exit(0);
        } else {
            // Fork multiplication process
            pid_mul = fork();
            if (pid_mul == -1) {
                perror("Error forking multiplication process");
                exit(EXIT_FAILURE);
            } else if (pid_mul == 0) {  // Multiplication child process
                signal(SIGALRM, signal_handler);
                close(pfd_to_child[1]);  // Close write end of pipe to child
                close(pfd_to_parent[0]); // Close read end of pipe to parent
                pause();                 // Wait for signal
                exit(0);
            } else {
                // Parent process
                close(pfd_to_child[0]);  // Close read end of pipe to child
                close(pfd_to_parent[1]); // Close write end of pipe to parent

                char input[SIZE];
                int num1, num2;
                char op;

                while (1) {
                    printf("Enter two integers and an operation (+, -, *) or 'q' to quit: ");
                    fgets(input, SIZE, stdin);

                    if (input[0] == 'q') {
                        break;
                    }

                    sscanf(input, "%d %d %c", &num1, &num2, &op);

                    // Send data to child
                    int nums[2] = {num1, num2};
                    write(pfd_to_child[1], nums, sizeof(nums));

                    // Signal appropriate child
                    if (op == '+') {
                        printf("The child process with PID: %d will provide the result.\n", pid_add);
                        kill(pid_add, SIGUSR1);
                    } else if (op == '-') {
                        printf("The child process with PID: %d will provide the result.\n", pid_sub);
                        kill(pid_sub, SIGUSR2);
                    } else if (op == '*') {
                        printf("The child process with PID: %d will provide the result.\n", pid_mul);
                        kill(pid_mul, SIGALRM);
                    } else {
                        printf("Invalid operation.\n");
                        continue;
                    }

                    // Read result from child
                    int result;
                    read(pfd_to_parent[0], &result, sizeof(result));
                    printf("Result: %d\n\n", result);
                }

                // Close pipes
                close(pfd_to_child[1]);
                close(pfd_to_parent[0]);

                // Wait for child processes to finish
                wait(NULL);
                wait(NULL);
                wait(NULL);
            }
        }
    }
    return 0;
}

void signal_handler(int signum) {
    int nums[2];

    // Read data from parent
    read(pfd_to_child[0], nums, sizeof(nums));

    int result;
    if (signum == SIGUSR1) {
        // Addition
        result = nums[0] + nums[1];
    } else if (signum == SIGUSR2) {
        // Subtraction
        result = nums[0] - nums[1];
    } else if (signum == SIGALRM) {
        // Multiplication
        result = nums[0] * nums[1];
    } else {
        printf("Unknown signal received.\n");
        exit(EXIT_FAILURE);
    }

    // Send result to parent
    write(pfd_to_parent[1], &result, sizeof(result));

    exit(0);  // Terminate child process after sending result
}
