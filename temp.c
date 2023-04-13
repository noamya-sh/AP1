#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "errno.h"
#include <signal.h>
#include <ctype.h> // for isalpha

#define REDIRECT_OUT 1
#define REDIRECT_ERR 2
#define REDIRECT_APP 3
#define MAX_COMMANDS 20
#define MAX_COMMAND_LENGTH 1024
#define MAX_VAR_NAME_LEN 20
#define MAX_VAR_VALUE_LEN 50
#define HASH_TABLE_SIZE 100

typedef struct variable {
    char name[MAX_VAR_NAME_LEN];
    char value[MAX_VAR_VALUE_LEN];
    struct variable *next;
} Variable;

Variable *hash_table[HASH_TABLE_SIZE];

// Hash function for strings
unsigned int hash_string(const char *str)
{
    unsigned int hash = 0;
    while (*str) {
        hash = hash * 31 + *str;
        str++;
    }
    return hash % HASH_TABLE_SIZE;
}

// Get a variable from the hash table
Variable *get_variable(const char *name){
    unsigned int hash = hash_string(name);
    Variable *var = hash_table[hash];
    while (var != NULL) {
        if (strcmp(var->name, name) == 0) {
            return var;
        }
        var = var->next;
    }
    return NULL;
}

// Set the value of a variable
void set_variable(const char *name, const char *value){
    Variable *var = get_variable(name);
    if (var == NULL) {
        // Variable does not exist, create a new one
        unsigned int hash = hash_string(name);
        var = (Variable *) malloc(sizeof(Variable));
        strncpy(var->name, name, MAX_VAR_NAME_LEN);
        var->next = hash_table[hash];
        hash_table[hash] = var;
    }
    strncpy(var->value, value, MAX_VAR_VALUE_LEN);
//    printf("%s, %s\n",var->name, var->value);
}
void freeHashTable() {
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        Variable * currNode = hash_table[i];
        while (currNode != NULL) {
            Variable* nextNode = currNode->next;
            free(currNode);
            currNode = nextNode;
        }
    }
}

char *last_command = "", *prompt_name = "hello";
char* commands[MAX_COMMANDS]; // Array to store command history
int num_commands = 0; // Number of commands in history
char input[MAX_COMMAND_LENGTH] = ""; // Input buffer for current command
int input_length = 0; // Length of input buffer
int command_index = 0; // Index of currently displayed command
char *outfile;
int fd, amper,redirect,retid,status,changed_prompt,changed_last;

// Function to read a single character from terminal without waiting for Enter key
char getch(){
    char buf = 0;
    struct termios old = {0};
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");
    old.c_lflag &= ~ICANON;
    old.c_lflag &= ~ECHO;
    old.c_cc[VMIN] = 1;
    old.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &old) < 0)
        perror("tcsetattr ICANON");
    if (read(0, &buf, 1) < 0)
        perror("read()");
    old.c_lflag |= ICANON;
    old.c_lflag |= ECHO;
    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror("tcsetattr ~ICANON");
    return buf;
}
int countCharOccurrences(const char* str, char c) {
    int count = 0;
    while (*str != '\0') {
        if (*str == c) {
            count++;
        }
        str++;
    }
    return count;
}
// Function to display a command in the shell
void display_command(char* command){
    printf("\r\033[K"); // Clear the current line
    printf("%s:%s",prompt_name, command);
    fflush(stdout);
}
int parser(char*** argv,char* str, int idx){
    int i = 0;
    argv[idx] = (char **) malloc(10 * sizeof (char *));
    // Remove trailing spaces at the end of the input string
    int len = strlen(str);
    while (len > 0 && str[len - 1] == ' ') {
        str[len - 1] = '\0';
        len--;
    }
    char *token;
    if (*str == ' ') str++;
    token = strtok(str," ");
    while (token != NULL){
        argv[idx][i] = token;
        token = strtok(NULL, " ");
        i++;
    }
    argv[idx][i] = NULL;
    return i;
}
void cleanInput(){
    input[0] = '\0';
    input_length = 0;

    printf("%s:",prompt_name);
    fflush(stdout);
}
void sigint_handler(int signum) {
    printf("You typed Control-C!\n");
    printf("%s:",prompt_name);
    fflush(stdout);
}
int main(){
    char** pipe_commands;
    char*** args;

    signal(SIGINT, sigint_handler);
    printf("Welcome to MyShell!\n");
    printf("%s:",prompt_name);
    fflush(stdout);
    //save orginal stdin, stdout
    int orig_stdin = dup(0);
    int orig_stdout = dup(1);
    while (1){
        char c = getch(); // Read a character from terminal

        if (c == '\n'){
            // Enter key pressed, execute command
            printf("\n");
            if (strcmp(input, "quit") == 0){
                break; // Exit the loop and terminate the program
            }
            if (strncmp("if", input, 2) == 0){
//                if_else();
                continue; // Exit the loop and terminate the program
            }
            else{
                if (strcmp(input, "!!") == 0){
                    if (!changed_last){
                        printf("No command has been executed yet");
                    }
                    else{
                        strncpy(input, last_command, strlen(last_command));
                    }
                }
                // Add command to command history
                commands[num_commands] = strdup(input);
                num_commands++;
                command_index = num_commands;

                int num_pipes = countCharOccurrences(commands[num_commands-1],'|');
                int pipesfd[num_pipes][2];
                for (int i = 0; i < num_pipes; i++) {
                    if (pipe(pipesfd[i]) == -1) {
                        perror("pipe");
                        exit(1);
                    }
                }
                pipe_commands = (char**) malloc((num_pipes + 1) * sizeof (char *));
                char* temp_input = strdup(input);
                char *token;
                token = strtok(temp_input,"|");
                int k = 0;
                while (token != NULL){
                    pipe_commands[k++] = token;
                    token = strtok(NULL, "|");
                }
                int pid,i;
                args = (char ***) malloc((num_pipes + 1) * sizeof(char **));
                for (int j = 0; j < num_pipes + 1; ++j) {

                    i = parser(args,pipe_commands[j],j);
                    if (args[j][0] == NULL)
                        break;
                    /* Does command line end with & */
                    if (i>0 && !strcmp(args[j][i - 1], "&")) {
                        amper = 1;
                        args[j][i - 1] = NULL;
                    }
                    else
                        amper = 0;

                    if (i > 3 && args[j][1][0] == '$' && !strcmp(args[j][i - 2], "=")){   //Q10 ---not finished
                        set_variable(args[j][i-3]+1,args[j][i-1]);
                        continue;
                    }


                    if (! strcmp(args[j][0], "read")){
                        input[0] = '\0';
                        input_length = 0;
                        char command[MAX_COMMAND_LENGTH];
                        fgets(command, 1024, stdin);
                        command[strlen(command)-1] = '\0';
                        char new_word[20]; // allocate space for the new word
                        strncpy(new_word, args[j][1], 20); // concatenate the original word to the new word
                        set_variable(new_word,command);
                        continue;
                    }


                    if (! strcmp(args[j][0], "echo")){   //Q3 && Q4
                        if(args[j][1][0]=='$'){
                            if(args[j][1][1]=='?'){
                                system("echo $?");
                            }
                            else{
                                Variable *var = get_variable(args[j][1]+1);
                                if (var)
                                    printf("%s\n", var->value);
                            }
                        }
                        else {
                            for (int s = 1; s < i; s++) {
                                printf("%s ", args[j][s]);
                            }
                            printf("\n");
                        }
                        continue;
                    } else if (i>2 && ! strcmp(args[j][i - 3], "prompt") && (! strcmp(args[j][i - 2], "="))) {  //Q2
                        if (changed_prompt) free(prompt_name);
                        prompt_name = strdup(args[j][i - 1]);
                        changed_prompt = 1;
                        continue;
                    } else if (i>1 && ! strcmp(args[j][0], "cd")) {   //Q5
                        chdir(args[j][1]);
                        continue;
                    }
                    if (i > 1 && !strcmp(args[j][i - 2], ">>")){    //Q1.2
                        redirect = REDIRECT_APP;
                        args[j][i - 2] = NULL;
                        outfile = args[j][i - 1];
                    }
                    else if (i > 1 && !strcmp(args[j][i - 2], ">")) {
                        redirect = REDIRECT_OUT;
                        args[j][i - 2] = NULL;
                        outfile = args[j][i - 1];
                    }
                    else if (i > 1 && !strcmp(args[j][i - 2], "2>")) {    //Q1.1
                        redirect = REDIRECT_ERR;
                        args[j][i - 2] = NULL;
                        outfile = args[j][i - 1];
                    }
                    else{
                        redirect = 0;
                    }
                    if ((pid = fork()) == -1) {
                        perror("fork");
                        exit(1);
                    }
                    else if (pid == 0) {
//                        if (num_pipes > 0 && j==0){
//                            close(STDOUT_FILENO); // close stdout
//                            dup2(pipesfd[j][1], STDOUT_FILENO); // redirect stdout to write end of first pipe */
//                            close(pipesfd[j][0]); // close read end of first pipe
//                        }
                        if (j > 0) {
                            dup2(pipesfd[j-1][0], 0);
                            close(pipesfd[j-1][0]);
                            close(pipesfd[j-1][1]);
                        }
                        if (j < num_pipes) {
                            // Redirect stdout to write end of pipe
                            dup2(pipesfd[j][1], 1);
                            close(pipesfd[j][0]);
                            close(pipesfd[j][1]);
                        }

                        /* redirection of IO ? */
                        if (redirect == REDIRECT_OUT) {
                            fd = creat(outfile, 0660);
                            close(STDOUT_FILENO) ;
                            dup(fd);
                            close(fd);

                            /* stdout is now redirected */
                        }
                        if (redirect == REDIRECT_ERR){
                            if (freopen(outfile, "w", stderr) == NULL) {
                                perror("freopen error");
                                return 1;
                            }
                            // Restore stderr to its original stream
                            if (freopen("/dev/stderr", "w", stderr) == NULL) {
                                perror("freopen error");
                                return 1;
                            }
                        }
                        if (redirect == REDIRECT_APP){
                            fd = open(outfile, O_CREAT | O_APPEND | O_RDWR, 0660);
                            close(STDOUT_FILENO) ;
                            dup(fd);
                            close(fd);
                        }
                        execvp(args[j][0], args[j]);
                    } else {
                        // Parent process
                        if (j < num_pipes) {
                            // Close write end of pipe
                            close(pipesfd[j][1]);
                        }
                        if (j > 0) {
                            // Close read end of previous pipe
                            close(pipesfd[j-1][0]);
                            close(pipesfd[j-1][1]);
                        }
                        retid = pid;
                        waitpid(pid, &status, 0);
                    }

                    if (amper == 0)
                        retid = wait(&status);
                }
                //redirect stdin and stdout to originals fds
                dup2(orig_stdin, 0);
                dup2(orig_stdout, 1);

                for (int i = 0; i < num_pipes+1; i++) {
                    free(args[i]);
                }

                free(args);
                free(pipe_commands);
                free(temp_input);
                if (changed_last) free(last_command);
                last_command = strdup(commands[num_commands-1]);
                changed_last = 1;
                cleanInput();
            }
            continue;
        }
        else if (c == 127){
            // If Backspace key is pressed, remove the last character from input buffer
            if (input_length > 0){
                input[--input_length] = '\0';
                display_command(input);
            }
        }
        else if (c == 27)
        {
            // If Esc key is pressed, check for arrow keys
            c = getch();
            if (c == '['){
                c = getch();
                if (c == 'A'){
                    // If Up arrow key is pressed, display the previous command in history
                    if (command_index > 0){
                        command_index--;
                        strncpy(input, commands[command_index], MAX_COMMAND_LENGTH - 1);
                        input_length = strlen(input);
                        display_command(input);
                    }
                }
                else if (c == 'B'){
                    // If Down arrow key is pressed, display the next command in history
                    if (command_index < num_commands){
                        command_index++;
                        if (command_index == num_commands){
                            // If at the end of command history, clear the input buffer
                            input[0] = '\0';
                            input_length = 0;
                        }
                        else{
                            strncpy(input, commands[command_index], MAX_COMMAND_LENGTH - 1);
                            input_length = strlen(input);
                        }
                        display_command(input);
                    }
                }
            }
        }
        else if (c >= 32 && c <= 126 && input_length < MAX_COMMAND_LENGTH - 1){
            // If a printable character is pressed, append it to input buffer
            input[input_length++] = c;
            input[input_length] = '\0';
            display_command(input);
        }
    }

    // Free memory allocated for command history
    for (int i = 0; i < num_commands; i++){
        free(commands[i]);
    }
    if (changed_prompt)free(prompt_name);
    if (changed_last) free(last_command);
    freeHashTable();
    return 0;
}