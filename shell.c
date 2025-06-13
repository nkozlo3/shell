#include "format.h"
#include "shell.h"
#include "vector.h"
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <bits/getopt_core.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysinfo.h>

#define SIGINT 2

/**
 * 1. Create a list of the features I need to implement. (as well as the gotchas)
 * 2. Plan out the entirety of the program.
 * 3. Implement the features one by one. Test each feature as you go.
 * 4. Re-test after implementing each feature.
 * 5. Done.
 */

/**
 * Features to implement:
 * 1. Starting up a shell.
 * 2. Optional arguments when launching shell
 * 3. Interaction
 * 4. Built-in commands
 * 5. Foreground external commands
 * 6. Logical operators
 * 7. SIGINT handling
 * 8. Exiting
 *
 *
 * 1. Background external commands
 * 2. ps
 * 3. Redirection commands
 * 4. Signal commands
 */

// inline declarations
void handle_history_file(char *filename);
void handle_file_commands(char *filename);
void execute_command(char *command);
int execute_builtin_command(char *command);
int is_builtin_command(char *command);
int execute_external_command(char *command);
void exit_shell();
int execute_cd(char *path);
int execute_history();
int execute_n(int n);
int execute_prefix(char *prefix);
void trim_space(char *str);
void add_background_job(pid_t pid);
void remove_background_job(pid_t pid);
void kill_process(pid_t pid);
void stop_process(pid_t pid);
void continue_process(pid_t pid);
int execute_ps();
process_info *get_process_info(pid_t pid);
void destroy_process_info(process_info *info);

typedef struct process
{
    char *command;
    pid_t pid;
    vector *previous_commands; // vector for previous commands
    char *history_file;        // History command file
    FILE *command_file;        // Command file
    vector *background_jobs;   // vector for background jobs will hold the int values of pids of the background jobs
    vector *background_jobs_commands;
    char *output_file;
    char *input_file;
    char *append_file;
} process;

static process *current_process; // the current process

/**
 * This function will handle the SIGINT signal.
 * It will kill any foreground jobs (which will happen anyway) so let's do nothing
 * and let the other processes be killed by SIGINT.
 */
void handle_sigint(int sig)
{
    if (!sig)
        printf("hi");
    // kill background jobs
}

/**
 * This function is the entry point for the shell program.
 *
 * Initialize current_process
 * Handle SIGINT using signal(). Call handle_sigint() when SIGINT is caught.
 *
 * Check for the optional arguments -h and -f with getopt before starting the shell.
 * - When provided -h the shell should load in the history file as its history using handle_history_file()
 * - When provided -f the shell should execute the commands in the script file, using handle_file_commands()
 *
 *
 * Start up a shell.
 * Starting up the shell will happen in the main function.
 * It will happen here because the shell will run in a loop until the user exits the shell.
 * The loop will go like this:
 *  - Print a command prompt.
 *  - Read a command from standard input. (to accept standard input we will use fgets() to read the command from standard input.)
 *  - Allow execute_command() to deal with the nuances of the command.
 *      - Print the PID of the process executing the command inside execute_command_external() if the command is an external command.
 *      - If the command is a built-in command, execute it using execute_builtin_command()
 *
 * When the shell is prompting for commands, it should print a prompt in the format:
 * (pid=<pid>)<path>$ // note that there is no newline at the end of the prompt.
 * We will call print_prompt() to print the prompt then wait for the user to enter a command
 * from standard input (or a file if -f is specified).
 *
 * TODO: avoid creating zombies. To do this, periodically call waitpid with the WNOHANG option in the main loop.
 * If a there is a hanging child, call remove_background_job().

 *
 * @param argc The number of arguments.
 * @param argv The array of arguments.
 * @return The exit status of the shell.
 */
int shell(int argc, char *argv[])
{

    // Handle SIGINT
    signal(SIGINT, handle_sigint);

    // Initialize the current process
    current_process = malloc(sizeof(process));
    current_process->background_jobs_commands = string_vector_create();
    current_process->previous_commands = string_vector_create();
    current_process->background_jobs = int_vector_create();
    current_process->history_file = NULL;
    current_process->command_file = NULL;
    current_process->output_file = NULL;
    current_process->input_file = NULL;
    current_process->append_file = NULL;
    current_process->pid = getpid();


    // Check and handle the optional arguments -h and
    int opt;
    while ((opt = getopt(argc, argv, "h:f:")) != -1)
    {
        switch (opt)
        {
        case 'h':

            handle_history_file(optarg);
            break;
        case 'f':
            handle_file_commands(optarg);
            break;
        }
    }

    // Start up the shell
    while (1)
    {
        // Print a command prompt.
        char *cwd = getcwd(NULL, 0);
        print_prompt(cwd, getpid());
        free(cwd);

        // Read a command from standard input.
        current_process->command = NULL;
        size_t len = 0;
        getline(&current_process->command, &len, stdin);

        // Handle EOF
        if (feof(stdin))
        {
            exit_shell();
        }

        // Execute the command.
        if (strcmp(current_process->command, "\n") != 0)
            execute_command(current_process->command);

        // Eithan Arelius (reap) background jobs to avoid zombies
        for (size_t i = 0; i < vector_size(current_process->background_jobs); i++)
        {
            int status;
            if (waitpid(*((int *)vector_get(current_process->background_jobs, i)), &status, WNOHANG) != 0)
            {
                remove_background_job(*((int *)vector_get(current_process->background_jobs, i)));
            }
        }
    }

    // Once we have the command, we will parse it and execute it.
    // this will be done in execute_command().

    // If the user enters the command "exit" or if it recieves an EOF, the shell should exit.
    // if there are any currently stopped or background jobs, the shell should kill and clean them up.
    // We should exit with a status of 0. This will be done in the exit_shell() function.

    // call our cleanup functions
    vector_destroy(current_process->previous_commands);
    vector_destroy(current_process->background_jobs);
    vector_destroy(current_process->background_jobs_commands);
    free(current_process->command);
    free(current_process);
    return 0;
}

/**
 * Optional argument the shell must support: -h <filename>
 * 1. -h <filename> - Load in the history file as its history. Upon exit, append the commands of the current session to the history file.
 *    This should work across directories. If the file does not exist, treat it as an empty file (create it).
 * In this function, handle loading the history file and creating it if it doesn't exist.
 */
void handle_history_file(char *filename)
{
    // Check if the file exists.
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        // file does not exist, create it.
        file = fopen(filename, "w");
        fclose(file);
    }
    else
    {
        // file exists, load it in as the history.
        // use fgets() to read the file line by line and add each line to the previous_commands vector.
        char *line = NULL;
        size_t len = 0;
        while (getline(&line, &len, file) != -1)
        {
            // add the line to the previous_commands vector.
            vector_push_back(current_process->previous_commands, line);
        }
        fclose(file);
    }

    current_process->history_file = filename;
}

/**
 * Optional argument the shell must support: -f <filename>
 * 2. -f <filename> - both print and run the commands in the file in sequencial order until the end of the file.
 *    If the user supplies an incorrect number of arguments, or the script file cannot be found,
 *    the shell should print an error message and exit.
 *
 * In this function, handle reading the file, executing the commands in it, and handling errors.
 */
void handle_file_commands(char *filename)
{
    // Check if the file exists.
    FILE *file = fopen(filename, "r");
    current_process->command_file = file;
    if (file == NULL)
    {
        // file does not exist, print the appropriate error message and exit.
        print_script_file_error();
        exit(1);
    }
    else
    {
        // file exists, read the file line by line and execute each command using execute_command().
        char *line = NULL;
        size_t len = 0;
        while (getline(&line, &len, file) != -1)
        {
            // execute the command using execute_command().
            execute_command(line);
        }
    }
}

/**
 * This function will execute the command given by the user.
 * It will differentiate between built-in commands and external commands.
 * Built-in commands will be executed by the shell itself. External commands will be executed by a child process.
 * Commands will be space-seperated without any quotes or trailing spaces.
 *
 *
 * 1. Parse the command so we only send one command to execute_external_command() or execute_builtin_command() at a time.
 * We also want to parse the command to deal with the logical operators.
 *  - If we encounter the logical operator &&, (ie x && y) we should execute x, then check the exit status of x. If it is 0, execute y. If it is not 0, do not execute y.
 *  - If we encounter the logical operator ||, (ie x || y) we should execute x, then check the exit status of x. If it is 0, do not execute y. If it is not 0, execute y.
 *  - If we encounter the logical operator ;, (ie x; y) we should execute x, then execute y.
 *  - If there is a logical operator, we need to parse the command into multiple commands. However, if there is a background command, ie a single external command followed by " &", we should send the whole command to execute_external_command().
 *  - ie "ls -l &" should be sent to execute_external_command() as "ls -l &".
 *
 *  - If there is a redirection operator, we should send initialize the appropriate file pointers and send just the command to execute_external_command().
 *  - That is if there is the redirection operator >, >>, or <.
 *
 * 2. If there is a logical operator, execute the commands in the order specified above.
 *
 * 3. Check if the command is a built-in command.
 *    If it is, execute it using execute_builtin_command(). And return, the specific functions should handle saving the command in the previous_commands vector.
 *
 * 4. If the command is not a built-in command, execute it as an external command.
 *    Do this in execute_external_command().
 *
 * 5. Finally, save the command in the previous_commands vector (and the history file if -h is specified).
 *  - If the command is !history, #<n>, or !<prefix>, do not save it in the previous_commands vector or the history file.
 *  - If the command is a logical operator, save the whole command in the previous_commands vector or the history file.
 *
 * @param command The command to execute.
 */
void execute_command(char *command)
{
    char *copy_command = strdup(command);

    // 1. Parse the command using strtok() and store each individual command in a vector. Also, parse to check if this is a logical operator or a redirection operator.
    vector *commands = string_vector_create();
    char *logical_operator = NULL;
    char *token = strtok(copy_command, " ");
    char *current_command = NULL;
    while (token != NULL)
    {
        if (strcmp(token, "&&") == 0 || strcmp(token, "||") == 0)
        {
            logical_operator = token;
            if (current_command != NULL)
            {
                trim_space(current_command);
                vector_push_back(commands, current_command);
                current_command = NULL;
            }
            token = strtok(NULL, " ");
            continue;
        }
        if (token[strlen(token) - 1] == ';')
        {
            logical_operator = ";";
            token[strlen(token) - 1] = '\0';
            if (current_command != NULL)
            {
                current_command = realloc(current_command, strlen(current_command) + strlen(token) + 2);
                strcat(current_command, " ");
                strcat(current_command, token);
                trim_space(current_command);
                vector_push_back(commands, current_command);
                current_command = NULL;
            }
            else
            {
                current_command = strdup(token);
                trim_space(current_command);
                vector_push_back(commands, current_command);
                current_command = NULL;
            }
            token = strtok(NULL, " ");
            continue;
        }

        if (strcmp(token, ">") == 0)
        {
            token = strtok(NULL, " ");
            trim_space(token);
            current_process->output_file = token;
            vector_push_back(commands, current_command);
            break;
        }
        if (strcmp(token, ">>") == 0)
        {
            token = strtok(NULL, " ");
            trim_space(token);
            current_process->append_file = token;
            vector_push_back(commands, current_command);
            break;
        }
        if (strcmp(token, "<") == 0)
        {
            token = strtok(NULL, " ");
            trim_space(token);
            current_process->input_file = token;
            vector_push_back(commands, current_command);
            break;
        }
        if (strcmp(token, "&") == 0)
        {
            current_command = strcat(current_command, " ");
            current_command = strcat(current_command, token);
            token = strtok(NULL, " ");
            continue;
        }
        if (current_command == NULL)
        {
            current_command = strdup(token);
        }
        else
        {
            current_command = realloc(current_command, strlen(current_command) + strlen(token) + 2);
            strcat(current_command, " ");
            strcat(current_command, token);
        }
        token = strtok(NULL, " ");
        if (token == NULL)
        {
            trim_space(current_command);
            vector_push_back(commands, current_command);
        }
    }

    // 2. If there is a logical operator, execute the commands in the order specified above.
    if (logical_operator != NULL)
    {
        if (strcmp(logical_operator, "&&") == 0)
        {
            int exit_status = 0;
            // execute the first command
            if (is_builtin_command((char *)vector_get(commands, 0)))
            {
                exit_status = execute_builtin_command((char *)vector_get(commands, 0));
            }
            else
            {
                exit_status = execute_external_command((char *)vector_get(commands, 0));
            }
            // if the exit status is 0, execute the second command
            if (exit_status == 0)
            {
                if (is_builtin_command((char *)vector_get(commands, 1)))
                {
                    execute_builtin_command((char *)vector_get(commands, 1));
                }
                else
                {
                    execute_external_command((char *)vector_get(commands, 1));
                }
            }
            // else, do not execute the second command
        }
        else if (strcmp(logical_operator, "||") == 0)
        {
            int exit_status = 0;
            // execute the first command
            if (is_builtin_command((char *)vector_get(commands, 0)))
            {
                exit_status = execute_builtin_command((char *)vector_get(commands, 0));
            }
            else
            {
                exit_status = execute_external_command((char *)vector_get(commands, 0));
            }
            // if the exit status is 0, do not execute the second command
            // else, execute the second command
            if (exit_status != 0)
            {
                if (is_builtin_command((char *)vector_get(commands, 1)))
                {
                    execute_builtin_command((char *)vector_get(commands, 1));
                }
                else
                {
                    execute_external_command((char *)vector_get(commands, 1));
                }
            }
        }
        else if (strcmp(logical_operator, ";") == 0)
        {
            // execute the first command
            if (is_builtin_command((char *)vector_get(commands, 0)))
            {
                execute_builtin_command((char *)vector_get(commands, 0));
            }
            else
            {
                execute_external_command((char *)vector_get(commands, 0));
            }
            // execute the second command
            if (is_builtin_command((char *)vector_get(commands, 1)))
            {
                execute_builtin_command((char *)vector_get(commands, 1));
            }
            else
            {
                execute_external_command((char *)vector_get(commands, 1));
            }
        }
        // Save the command in the previous_commands vector (and the history file if -h is specified).
        trim_space(command);
        vector_push_back(current_process->previous_commands, command);
        if (current_process->history_file)
        {
            char *str_toPush = malloc(strlen(command) + 5);
            strcat(str_toPush, command);
            strcat(str_toPush, "\n");
            FILE *file = fopen(current_process->history_file, "a");
            fprintf(file, "%s", str_toPush);
            fclose(file);
            free(str_toPush);
        }
        // We are done with the logical operator, so we can free the commands vector and return.
        vector_destroy(commands);
        free(current_command);
        free(copy_command);
        return;
    }

    // 3. Check if the command is a built-in command and execute it. If it is not a built-in command, execute it as an external command.
    if (is_builtin_command((char *)vector_get(commands, 0)))
    {
        execute_builtin_command((char *)vector_get(commands, 0));
    }
    else
    {
        execute_external_command((char *)vector_get(commands, 0));
    }

    // 4. Finally, save the command in the previous_commands vector (and the history file if -h is specified).
    if (strcmp((char *)vector_get(commands, 0), "!history") != 0 && ((char *)vector_get(commands, 0))[0] != '#' && ((char *)vector_get(commands, 0))[0] != '!')
    {
        trim_space(command);
        vector_push_back(current_process->previous_commands, command);
        if (current_process->history_file)
        {
            char *str_toPush = malloc(strlen(command) + 5);
            strcat(str_toPush, command);
            strcat(str_toPush, "\n");
            FILE *file = fopen(current_process->history_file, "a");
            fprintf(file, "%s", str_toPush);
            fclose(file);
            free(str_toPush);
        }
    }
    // We are done with the command, so we can free the commands vector.
    vector_destroy(commands);
}

/**
 * This function will execute a built-in command.
 * @param command The command to execute.
 * @return The exit status of the command.
 */
int execute_builtin_command(char *command)
{
    // The built-in commands are: cd <path>, !history, #<n>, !<prefix>, exit.
    char *copy_command = strdup(command);
    char *token = strtok(copy_command, " ");
    if (strcmp(token, "cd") == 0) // This and everything below in this function was entirely auto-generated by Github Copilot.
    {
        token = strtok(NULL, " ");
        if (token == NULL)
        {
            print_invalid_command(copy_command);
            free(copy_command);
            return 1;
        }
        return execute_cd(token);
    }
    if (strcmp(token, "!history") == 0)
    {
        return execute_history();
    }
    else if (token[0] == '#')
    {
        int n = atoi(token + 1);
        return execute_n(n);
    }
    else if (token[0] == '!')
    {
        char *cmd = command + 1;
        return execute_prefix(cmd);
    }
    else if (strcmp(token, "exit") == 0)
    {
        exit_shell();
        return 0;
    }
    else if (strcmp(token, "kill") == 0)
    {
        token = strtok(NULL, " ");
        if (token == NULL)
        {
            print_invalid_command(command);
            return 1;
        }
        int pid = atoi(token);
        kill_process(pid);
        return 0;
    }
    else if (strcmp(token, "stop") == 0)
    {
        token = strtok(NULL, " ");
        if (token == NULL)
        {
            print_invalid_command(command);
            return 1;
        }
        int pid = atoi(token);
        stop_process(pid);
        return 0;
    }
    else if (strcmp(token, "cont") == 0)
    {
        token = strtok(NULL, " ");
        if (token == NULL)
        {
            print_invalid_command(command);
            return 1;
        }
        int pid = atoi(token);
        continue_process(pid);
        return 0;
    }
    else if (strcmp(token, "ps") == 0)
    {
        return execute_ps();
        return 0;
    }

    print_invalid_command(command);

    return 1;
}

/**
 * This function will handle exiting the shell.
 * It will kill and clean up any stopped or background jobs.
 * It will exit with a status of 0.
 */
void exit_shell()
{

    // Kill and clean up any stopped or background jobs.
    for (size_t i = 0; i < vector_size(current_process->background_jobs); i++)
    {
        kill_process(*((int *)vector_get(current_process->background_jobs, i)));
    }
    // TODO: clean up memory
    free(current_process->command);
    free(current_process);

    // Exit with a status of 0.
    exit(0);
}

/**
 * This function will execute the cd command.
 * @param path The path to change to.
 * @return The exit status of the command.
 */
int execute_cd(char *path)
{
    // paths not starting with / should be relative to the current directory.
    // If the path is not valid, send the appropriate error message.

    // Use chdir() to change the directory to the given path.
    if (path[0] != '/')
    {
        char *current_directory = getcwd(NULL, 0);
        char *new_path = malloc(strlen(current_directory) + strlen(path) + 2);
        new_path[0] = '\0';
        strcat(new_path, current_directory);
        strcat(new_path, "/");
        strcat(new_path, path);
        if (chdir(new_path) == -1)
        {
            print_no_directory(path);
            free(new_path);
            return 1;
        }
        free(new_path);
        return 0;
    }
    chdir(path);
    if (chdir(path) == -1)
    {
        print_no_directory(path);
        return 1;
    }
    return 0;
}

/**
 * This function will execute the !history command.
 *  This command will not be saved in the previous_commands vector or the history file.
 * @return The exit status of the command.
 */
int execute_history()
{

    // Print out each command in the previous_commands vector in order.
    for (size_t i = 0; i < vector_size(current_process->previous_commands); i++)
    {
        print_history_line(i, (char *)vector_get(current_process->previous_commands, i));
    }

    return 0;
}

/**
 * This function will print and execute the #<n> command.
 * This is in chronological order from earliest to most recent.
 * @param n The number of the command to execute.
 * @return The exit status of the command.
 */
int execute_n(int n)
{

    // If n is not a valid index, print the appropriate error and leave this function.
    if (n < 0 || (size_t)n >= vector_size(current_process->previous_commands))
    {
        print_invalid_index();
        return 1;
    }

    // Else, print the command and execute it using execute_command().
    print_command((char *)vector_get(current_process->previous_commands, n));
    execute_command((char *)vector_get(current_process->previous_commands, n));

    return 0;
}

/**
 * This function will print and execute the most recent command with the given prefix.
 * This is in chronological order from earliest to most recent.
 * The prefix may be empty in which case the most recent command will be executed.
 *
 * If there is no command with the given prefix, print the appropriate error and leave this function.
 *
 * @param prefix The prefix of the command to execute.
 * @return The exit status of the command.
 */
int execute_prefix(char *prefix)
{
    for (int i = (int)vector_size(current_process->previous_commands) - 1; i >= 0; i--) // Github Copilot generated this for loop and its contents.
    {
        if (strncmp(prefix, (char *)vector_get(current_process->previous_commands, (size_t)i), strlen(prefix)) == 0)
        {
            print_command((char *)vector_get(current_process->previous_commands, (size_t)i));
            execute_command((char *)vector_get(current_process->previous_commands, (size_t)i));
            return 0;
        }
    }

    print_no_history_match();
    return 1;
}

/**
 * This function will execute an external command.
 *
 * If any of fork, exec, or wait fail, print the appropriate error message.
 * The child should exit with a status of 1 if it fails to execute the command.
 *
 * IMPORTANT: To prevent fork bombing the autograder, make sure to have an exit statement after the exec*() call (the code the child process will execute).
 * IMPORTANT: flush the standard output stream before forking, using fflush(stdout).
 * IMPORTANT: After forking, use setgpid() to set each child process to a new process group.
 *
 * Use fork() to create a child process and execute the command in the child process.
 *
 * Print the pid of the child process using print_command_executed() (format.h).
 * The child process must execute the command using exec*() functions.
 * The parent process must wait for the child process to finish using waitpid().
 *
 * @param command The command to execute.
 * @return The exit status of the command.
 */
int execute_external_command(char *command)
{
    // If the command tells us to start a background process, do not wait for the child process to finish.
    // ie if (background) waitpid() should not be called. else it should be called. Additionally, we should run add_background_job() to add the child process to the background_jobs vector.
    int background = 0;
    if (command[strlen(command) - 1] == '&')
    {
        background = 1;
        command[strlen(command) - 1] = '\0';
    }

    fflush(stdout);
    pid_t pid = fork();
    if (pid < 0)
    {
        // Fork failed
        print_fork_failed();
        return 1;
    }
    else if (pid > 0)
    {
        // Parent process

        if (!background)
        {
            int status;
            waitpid(pid, &status, 0);

            // if waitpid fails, print the appropriate error message
            if (!WIFEXITED(status))
            {
                print_wait_failed();
                return 1;
            }
            if (WEXITSTATUS(status) != 0)
            {
                return 1;
            }
        }
        else
        {
            // set the child process to a new process group
            setpgid(pid, pid);

            // add the child process to the background_jobs vector
            add_background_job(pid);
            vector_push_back(current_process->background_jobs_commands, command);
        }
    }
    else if (pid == 0)
    {
        // Child process
        // print the pid of the child process
        print_command_executed(getpid());
        int fd = -1;
        if (current_process->output_file)
        {
            mode_t mode = S_IRUSR | S_IWUSR;
            fd = open(current_process->output_file, O_RDWR | O_CREAT, mode);
            if (fd == -1)
            {
                print_redirection_file_error(); 
                exit(1);
            }
            else
            {
                dup2(fd, 1);
            }
        }

        if (current_process->append_file)
        {
            close(1);
            mode_t mode = S_IRUSR | S_IWUSR;
            fd = open(current_process->append_file, O_RDWR | O_APPEND | O_CREAT, mode);
            if (fd == -1)
            {
                print_redirection_file_error();
                exit(1);
            }
            else
            {
                dup2(fd, 1);
            }
        }
        if (current_process->input_file) // Github Copilot helped do this.
        {
            int fd = open(current_process->input_file, O_RDONLY);
            if (fd == -1)
            {
                print_redirection_file_error();
                exit(1);
            }
            if (dup2(fd, STDIN_FILENO) == -1)
            {
                print_redirection_file_error();
                exit(1);
            }
            close(fd);
        }

        // Get the command and the arguments // Github Copilot helped do this.
        char **args = malloc(sizeof(char *));
        char *token = strtok(command, " ");
        int i = 0;
        while (token != NULL)
        {
            args[i] = token;
            token = strtok(NULL, " ");
            i++;
            args = realloc(args, (i + 1) * sizeof(char *));
        }
        args[i] = NULL;
        // execute the command using exec*()
        if (execvp(args[0], args) == -1)
        {
            // exec failed
            free(args);
            print_exec_failed(command);
            exit(1);
        }
        if (fd)
            close(fd);
        free(args);
        exit(0); // Just to be safe
    }
    if (current_process->input_file)
    {
        current_process->input_file = NULL;
    }
    if (current_process->output_file)
    {
        current_process->output_file = NULL;
    }
    if (current_process->append_file)
    {
        current_process->append_file = NULL;
    }


    return 0;
}

/**
 * This function will add a background job to the background_jobs vector.
 * @param pid The pid of the background job.
 */
void add_background_job(pid_t pid)
{
    int *pid_ptr = malloc(sizeof(int)); // TODO: clean each of these up in our destructor
    *pid_ptr = pid;
    vector_push_back(current_process->background_jobs, pid_ptr);
}

/**
 * This function will remove a background job from the background_jobs vector.
 * @param pid The pid of the background job.
 */
void remove_background_job(pid_t pid)
{
    for (size_t i = 0; i < vector_size(current_process->background_jobs); i++)
    {
        if (*((int *)vector_get(current_process->background_jobs, i)) == pid)
        {
            vector_erase(current_process->background_jobs, i);
        }
    }
    kill_process(pid);
}

/**
 * This function implements the ps command.
 *
 * Print the pid, NLWP, VSZ, STAT, START, TIME, and COMMAND of each process in the background_jobs vector.
 * We can get the pid, NLWP, VSZ, STAT, START, and TIME by using the /proc filesystem.
 * As well as that of the main shell process.
 * @return The exit status of the command.
 */
int execute_ps()
{
    print_process_info_header();
    for (size_t i = 0; i < vector_size(current_process->background_jobs); i++)
    {
        process_info *info = get_process_info(*((int *)vector_get(current_process->background_jobs, i)));
        info->command = strdup((char *)vector_get(current_process->background_jobs_commands, i));
        print_process_info(info);
        destroy_process_info(info);
    }
    process_info *info = get_process_info(current_process->pid);
    info->command = strdup("./shell");
    print_process_info(info);
    destroy_process_info(info);
    return 0;
}

/**
 * This function will kill the specified process.
 * @param pid The pid of the process to kill.
 */
void kill_process(pid_t pid)
{
    if (kill(pid, SIGKILL) == -1)
    {
        print_no_process_found(pid);
    }
    else
    {
        print_killed_process(pid, "command");
    }
}

/**
 * This function will allow the shell to stop a currently running process.
 * Use SIGSTOP to stop the process. It may be resumed by using cont.
 * @param pid The pid of the process to stop.
 */
void stop_process(pid_t pid)
{
    // If no process with the given pid exists, print the appropriate error message.
    // Else, use send SIGSTOP to the process.
    if (kill(pid, SIGSTOP) == -1)
    {
        print_no_process_found(pid);
    }
    else
    {
        print_stopped_process(pid, "command");
    }
}

/**
 * This function will allow the shell to continue a currently stopped process.
 * Use SIGCONT to continue the process.
 * @param pid The pid of the process to continue.
 */
void continue_process(pid_t pid)
{
    if (kill(pid, SIGCONT) == -1)
    {
        print_no_process_found(pid);
    }
    else
    {
        print_continued_process(pid, "command");
    }
}

/**
 * This function will return 1 if the command is a built-in command, 0 otherwise.
 * @param command The command to check.
 * @return 1 if the command is a built-in command, 0 otherwise.
 */
int is_builtin_command(char *command)
{
    char *copy_command = strdup(command);
    char *token = strtok(copy_command, " ");
    trim_space(token);
    if (strcmp(token, "stop") == 0 || strcmp(token, "cont") == 0 || strcmp(token, "kill") == 0 || strcmp(token, "exit") == 0 || strcmp(token, "cd") == 0 || strcmp(token, "!history") == 0 || token[0] == '#' || token[0] == '!' || strcmp(token, "ps") == 0)
    {
        free(copy_command);
        return 1;
    }
    free(copy_command);
    return 0;
}

/**
 * Helper function to trim leading whitespace, newline, and carriage return characters.
 * @param str The string to trim.
 */
void trim_space(char *str)
{
    size_t len = strlen(str);
    if (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\n' || str[len - 1] == '\r'))
    {
        str[len - 1] = '\0';
    }
}

process_info *get_process_info(pid_t pid)
{
    process_info *info = malloc(sizeof(process_info));
    info->pid = pid;
    char *path = malloc(500);
    sprintf(path, "/proc/%d/status", info->pid);
    FILE *file = fopen(path, "r");
    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, file) != -1)
    {
        if (strncmp(line, "VmSize:", 7) == 0)
        {
            info->vsize = atol(line + 7);
        }
        if (strncmp(line, "Threads:", 8) == 0)
        {
            info->nthreads = atol(line + 8);
        }
    }
    fclose(file);
    // Similar process but we are going to /proc/pid/stat
    sprintf(path, "/proc/%d/stat", info->pid);
    file = fopen(path, "r");
    getline(&line, &len, file);
    char *token = strtok(line, " ");
    for (int i = 1; i < 23; i++)
    {
        if (i == 3)
        {
            info->state = token[0];
        }
        if (i == 14)
        {
            long time_seconds = atol(token) / sysconf(_SC_CLK_TCK);
            struct tm *time_since_start = gmtime(&time_seconds);
            char buffer[26];
            time_struct_to_string(buffer, 26, time_since_start);
            info->time_str = strdup(buffer);
            // info->time_str = strdup(token);
        }
        if (i == 22)
        {
            struct sysinfo sys_info; // Github Copilot helped me to fix my time by adding the boot_time logic.
            sysinfo(&sys_info);
            time_t boot_time = time(NULL) - sys_info.uptime;
            // convert the start time to a human readable format
            time_t start_time = boot_time + atol(token) / sysconf(_SC_CLK_TCK);
            struct tm *tm_info = localtime(&start_time);
            char buffer[26];
            time_struct_to_string(buffer, 26, tm_info);
            info->start_str = strdup(buffer);
        }
        token = strtok(NULL, " ");
    }
    fclose(file);
    free(path);
    free(line);
    return info;
}

void destroy_process_info(process_info *info)
{
    free(info->time_str);
    free(info->start_str);
    free(info->command);
    free(info);
}