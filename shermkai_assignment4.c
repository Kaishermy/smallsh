#include <stdio.h>
#include <stdlib.h> // for EXIT_SUCCESS and EXIT_FAILURE
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h> 
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

// Adapted from Programming Assignment 4 sample parser code
#define INPUT_LENGTH 2048
#define MAX_ARGS 512
#define PID_DEFAULT -5

// Adapted from Programming Assignment 4 sample parser code
struct command_line {
	char* argv[MAX_ARGS + 1];
	int argc;
	char* input_file;
	char* output_file;
	bool is_bg;
};

// Adapted from Programming Assignment 4 sample parser code
struct command_line* parse_input() {
	char input[INPUT_LENGTH];
	struct command_line* curr_command = (struct command_line*)calloc(1,
		sizeof(struct command_line));

	// Get input
	printf(": ");
	fflush(stdout);
	fgets(input, INPUT_LENGTH, stdin);

	// Tokenize the input
	char* token = strtok(input, " \n");
	while (token) {
		if (!strcmp(token, "<")) {
			curr_command->input_file = strdup(strtok(NULL, " \n"));
		}
		else if (!strcmp(token, ">")) {
			curr_command->output_file = strdup(strtok(NULL, " \n"));
		}
		else if (!strcmp(token, "&")) {
			curr_command->is_bg = true;
		}
		else {
			curr_command->argv[curr_command->argc++] = strdup(token);
		}
		token = strtok(NULL, " \n");
	}

	return curr_command;
}

int main() {
	struct command_line* curr_command;
	char* command;
	int exitStatus = 0;
	int childStatus;
	int bgDefaultInFD = open("/dev/null", O_RDONLY, 0640);
	int bgDefaultOutFD = open("/dev/null", O_WRONLY | O_CREAT | O_TRUNC, 0640);

	int bgPIDs[MAX_ARGS + 1] = { 0 }; // The PIDs of any currently running background processes
	for (int i = 0; i < MAX_ARGS + 1; i++) bgPIDs[i] = PID_DEFAULT; // Fill w/ -5 to avoid accidental PID hits

	while (true) {
		curr_command = parse_input();
		command = curr_command->argv[0];

		// Re-prompt if nothing entered or comment entered
		if (command == NULL || strncmp(command, "#", 1) == 0) {
			continue;
		}
		else if (strcmp(command, "exit") == 0) {
			// TODO: Kill all processes/jobs and terminate
			break;
		}
		else if (strcmp(command, "cd") == 0) {
			char* newPath;

			if (curr_command->argv[1] == NULL) {
				newPath = getenv("HOME"); // Default to dir in HOME env var
			}
			else {
				newPath = curr_command->argv[1];
			}
			chdir(newPath);
		}
		else if (strcmp(command, "status") == 0) {
			printf("exit value %d\n", exitStatus);
			fflush(stdout);
		}

		// Any other commands go here (e.g. ls, pwd, etc.)
		else {
			int spawnPID = fork();

			if (spawnPID == -1) { // Child failure
				exitStatus = 1;
				exit(EXIT_FAILURE);
			}
			if (spawnPID == 0) { // Child success
				// Background-process redirect should go straight to /dev/null
 				if (curr_command->is_bg) {
					if (curr_command->input_file == NULL) {
						dup2(bgDefaultInFD, 0);
					}
					if (curr_command->output_file == NULL) {
						dup2(bgDefaultOutFD, 1);
					}
				}

				// Handle specified redirects
				if (curr_command->input_file != NULL) {
					// Open for reading only
					int sourceFD = open(curr_command->input_file, O_RDONLY, 0640);

					// Check if file open fails. Redirect if file opens successfully.
					if (sourceFD == -1 || dup2(sourceFD, 0) == -1) exitStatus = 1;
				}
				if (curr_command->output_file != NULL) {
					// Open for writing only and either truncated or created
					int targetFD = open(curr_command->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0640);

					// Check if file open fails. Redirect if file opens successfully.
					if (targetFD == -1 || dup2(targetFD, 1) == -1) exitStatus = 1;
				}

				// Run child process
				execvp(command, curr_command->argv);
				exitStatus = 0;
				exit(EXIT_SUCCESS);
			}
			else { // Parent Process
				int childStatus;

				// Either wait for child completion or immediately resume execution depending on
				// whether the child process is a foreground or background process
				if (!curr_command->is_bg) { // Child is a foreground process
					waitpid(spawnPID, &childStatus, 0);
				}
				else { // Child is background process
					printf("background pid is %d\n", spawnPID);
					fflush(stdout);
					
					// Add the child PID to the first open array slot
					for (int i = 0; i < MAX_ARGS + 1; i++) {
						if (bgPIDs[i] == PID_DEFAULT) {
							bgPIDs[i] = spawnPID;
							break;
						}
					}
				}
			}
		}

		// Go through each running PID, check if it's process terminated, and kill it if so
		int currChildStatus;
		for (int i = 0; i < MAX_ARGS + 1; i++) {
			if (waitpid(bgPIDs[i], &currChildStatus, WNOHANG) > 0) {
				printf("background pid %d is done: exit value %d\n", bgPIDs[i], exitStatus);
				fflush(stdout);
				bgPIDs[i] = PID_DEFAULT;
			}
		}
	}

	return EXIT_SUCCESS;

}