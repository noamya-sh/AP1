#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include "stdio.h"
#include "errno.h"
#include "stdlib.h"
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <termios.h>

void sigint_handler(int signum) {
    printf("You typed Control-C!\n");
}

int main() {
char command[1024];
char *token;
char *outfile;
int exit = 0, i, fd, amper, redirect, retid, status,c1=0;
char *argv[10];
char *prompt_name = "hello";
char variable[] = "VARIABLE=value";
char c;
signal(SIGINT, sigint_handler);   //Q8   ---- need to check about another programs (and it's show the word '^C')

struct termios old_attr, new_attr;

while (exit==0)
{

    fgets(command, 10234, stdin);
    command[strlen(command) - 1] = '\0';
//    if (command[0] == '\033'){
//        printf("kk key up");
//        if (command[0] == KEY_UP) {
//            printf("hi key up");
//        }
//    }
    /* parse command line */
    i = 0;
    token = strtok (command," ");
    while (token != NULL)
    {
        argv[i] = token;
        token = strtok (NULL, " ");
        i++;
    }
    argv[i] = NULL;

    /* Is command empty */
    if (argv[0] == NULL)
        continue;

    /* Does command line end with & */
    if (! strcmp(argv[i - 1], "&")) {
        amper = 1;
        argv[i - 1] = NULL;
    }
    else
        amper = 0;

    if (! strcmp(argv[i - 3], "prompt") && (! strcmp(argv[i - 2], "="))) {  //Q2
        prompt_name = argv[i - 1];
        continue;
    }

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
        continue;
    }

    if (! strcmp(argv[0], "cd")){   //Q5
        chdir(argv[1]);
        continue;
    }

    if (! strcmp(argv[0], "!!")) {  //Q6 ---- doesn't work yet
        system("!!");
        continue;
    }

    if (! strcmp(argv[0], "quit")){   //Q7
        exit = 1;
        continue;
    }

    if (argv[0][0] == '$'){   //Q10 ---not finished
        printf("var!!\n");
        continue;
    }

    if (! strcmp(argv[i - 2], ">>")){    //Q1.2  ---not finished
        FILE *file = fopen(argv[i-1],"a+");
        fprintf(file, argv[i-3]);
        fclose(file);
        continue;
        }


    if (! strcmp(argv[i - 2], ">")) {
        redirect = 1;
        argv[i - 2] = NULL;
        outfile = argv[i - 1];
        }
    else if (! strcmp(argv[i - 2], "2>")) {    //Q1.1
        redirect = 2;
        argv[i - 2] = NULL;
        outfile = argv[i - 1];
        }
    else{
        redirect = 0;
    }
    /* for commands not part of the shell command language */

    if (fork() == 0) {
        /* redirection of IO ? */
        if (redirect == 1) {
            fd = creat(outfile, 0660);
            close (STDOUT_FILENO) ;
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
    }
    /* parent continues here */
    if (amper == 0)
        retid = wait(&status);
}

}