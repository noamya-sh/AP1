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

#define MAX_COMMANDS 20
#define MAX_COMMAND_LENGTH 1024
char *prompt_name = "hello";
char* commands[MAX_COMMANDS]; // Array to store command history
int num_commands = 0; // Number of commands in history
char input[MAX_COMMAND_LENGTH] = ""; // Input buffer for current command
int input_length = 0; // Length of input buffer
int command_index = 0; // Index of currently displayed command
char *argv[10];
char *outfile;
int fd, amper,redirect,retid,status,changed_prompt;

// Function to read a single character from terminal without waiting for Enter key
char getch()
{
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

// Function to display a command in the shell
void display_command(char* command){
    printf("\r\033[K"); // Clear the current line
    printf("%s:%s",prompt_name, command);
    fflush(stdout);
}
int parser(){
    int i = 0;
    char *token;
    token = strtok (input," ");
    while (token != NULL)
    {
        argv[i] = token;
        token = strtok (NULL, " ");
        i++;
    }
    argv[i] = NULL;
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
    signal(SIGINT, sigint_handler);
    printf("Welcome to MyShell!\n");
    printf("%s:",prompt_name);
    fflush(stdout);
    while (1){
        char c = getch(); // Read a character from terminal

        if (c == '\n'){
            // Enter key pressed, execute command
            printf("\n");
            if (strcmp(input, "quit") == 0){
                // Check if input is "quit" command
                break; // Exit the loop and terminate the program
            }
            else{
                // Add command to command history
                commands[num_commands] = strdup(input);
//                char *temp = strdup(input);
                num_commands++;
                command_index = num_commands;
                // TODO: Execute command here


//
                int i = parser();
                if (argv[0] == NULL)
                    continue;
                if (!strcmp(input, "!!") && num_commands > 1){ //not working
                    printf("%d\n",num_commands);
                    strncpy(input, commands[num_commands-2], MAX_COMMAND_LENGTH - 1);
                    printf("%s\n",input);
                    input_length = strlen(input);
                    i = parser();
                    cleanInput();
                }
                /* Does command line end with & */
                if (i>0 && !strcmp(argv[i - 1], "&")) {
                    amper = 1;
                    argv[i - 1] = NULL;
                }
                else
                    amper = 0;

                if (! strcmp(argv[0], "echo")){   //Q3 && Q4
                    if(! strcmp(argv[1], "$?")){
                        system("echo $?");
                    }
                    else {
                        for (int j = 1; j < i; j++) {
                            printf("%s ", argv[j]);
                        }
                        printf("\n");
                    }
                    cleanInput();
                    continue;
                } else if (i>2 && ! strcmp(argv[i - 3], "prompt") && (! strcmp(argv[i - 2], "="))) {  //Q2
                    if (changed_prompt) free(prompt_name);
                    prompt_name = strdup(argv[i - 1]);
                    changed_prompt = 1;
                    cleanInput();
                    continue;
                } else if (i>1 && ! strcmp(argv[0], "cd")) {   //Q5
                    chdir(argv[1]);
                    cleanInput();
                    continue;
                }
                if (! strcmp(argv[i - 2], ">>")){    //Q1.2
                    FILE *file = fopen(argv[i-1],"a+");
                    fprintf(file, "%s", argv[i-3]);
                    fclose(file);
                    cleanInput();
                    continue;
                }

                if (i > 1 && !strcmp(argv[i - 2], ">")) {
                    redirect = 1;
                    argv[i - 2] = NULL;
                    outfile = argv[i - 1];
                }
                else if (i > 1 && !strcmp(argv[i - 2], "2>")) {    //Q1.1
                    redirect = 2;
                    argv[i - 2] = NULL;
                    outfile = argv[i - 1];
                }
                else{
                    redirect = 0;
                }
                if (fork() == 0) {
                    /* redirection of IO ? */
                    if (redirect == 1) {
//                        printf("jjjjjj");
                        fd = creat(outfile, 0660);
                        close(STDOUT_FILENO) ;
                        dup(fd);
                        close(fd);

                        /* stdout is now redirected */
                    }
                    if (redirect == 2){
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
                    execvp(argv[0], argv);
                    cleanInput();
                }
                if (amper == 0)
                    retid = wait(&status);
//                }
                // Reset input buffer and display prompt
                cleanInput();
            }
        }
        else if (c == 127)
        {
            // If Backspace key is pressed, remove the last character from input buffer
            if (input_length > 0)
            {
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
    if (changed_prompt)
        free(prompt_name);

    return 0;
}