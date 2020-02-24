/**
 * Simple shell interface starter kit program.
 * Operating System Concepts
 * Mini Project1
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define MAX_LINE		80 /* 80 chars per line, per command */

char * read_line(void) {
    char *line = malloc((MAX_LINE + 1) * sizeof(char));
    int i = 0; //count

    // if(line == NULL) {
    //     return NULL;
    // }

    while(1) {
        int c = fgetc(stdin);
        if(c == EOF) {
           break;
        }
        line[i] = c;
        if(line[i] == '\n') {
            line[i] = '\0';
            return line;
        }
        i++;   
    }
}

/* parses args
* returns a pointer of pointers (array of arrays)
*/
char ** parse_args(char * line, char delimiter) {
    char *cstr = (char *) malloc(MAX_LINE/2 +1); //current arg, 80
    char **args = malloc((MAX_LINE/2 + 1) * sizeof(char *));
    int argcount = 0; // amount of args
    int c = 0; //count of chars for each arg
    size_t len = strlen(line);

    for(int i = 0; i < len + 1 ; i++) {
        if(line[i] == '\0' && strlen(cstr) == 0) {
            //if end of string and current string is size 0, just return args
            free(cstr);
            return args;
        }
        if(line[i] == '\0'){
            cstr[c] = '\0';
            args[argcount] = malloc(sizeof(cstr));
            strcpy(args[argcount], cstr);
            memset(cstr,0,strlen(cstr)); //reset string
            free(cstr); //free memory
            return args;
        }
        //if current iteration is a space add 
        if(line[i] == delimiter) {    
            if(strlen(cstr) == 0) {
                continue;
            }
            cstr[c] = '\0';
            args[argcount] = malloc(sizeof(cstr));
            strcpy(args[argcount], cstr);
            memset(cstr,0,strlen(cstr)); //reset string
            c=0;
            argcount++;
            continue;
        }
        cstr[c] = line[i];
        c++;
    }
}

/* checking if arguments contain an ampersand (&) character
** if it does not contain the ampersand char, return -1
** if it does, return the index at which it exists
*/
int should_wait(char **args) {
    int count = 0;
    while(*args != NULL) {
        if(strcmp(*args, "&") == 0) {
            return count;
        }
        *args++;
        count++;
    }
    return -1;
}

//returns the index of the input file name if '<' exists, returns -1 if '<' is not in the arguments
// dir argument is for < (input) or > (output)
int get_file(char ** args, char * dir) {
    int count = 0;
    while (*args != NULL) {
        //printf("get_file: %s , %s\n", *args, dir);
        if(strcmp(*args, dir) == 0) {
            return ++count;
        }
        count++;
        *args++;
    }
    return -1;
}

//returns 1 if pipe exists in args, 0 if not.
int has_pipe(char ** args) {
    while(*args != NULL) {
        if(strcmp(*args, "|") == 0) {
            return 1;
        }
        *args++;
    }
    return 0;
}

int main(void) {
    char * line;
    char **args;
    char * histline = malloc((MAX_LINE + 1) * sizeof(char));
    int wait = -1; //flag for if parent process should wait for child process, by default always wait
    int should_run = 1;

    while (should_run) {
        printf("mysh:~$ "); //prevent child process from printing mysh:~$ again.
        fflush(stdout);
        //reading arguments.
        line = read_line();
        if (*line == '\0') {
            continue;
        }
        args = parse_args(line, ' ');
        wait = should_wait(args);
        //if wait is not -1 remove the existing ampersand by setting to null.
        if(wait != -1) {
            args[wait] = NULL;
        }

        //built ins
        if(strcmp(args[0], "exit") == 0) {
            printf("now exiting...\n");
            return 1;
        } else if(strcmp(args[0], "cd") == 0) {
            if(chdir(args[1]) < 0) {
                printf("%s\n", strerror(errno));
            }
            continue;
        } 
        
        //history feature
        if(strcmp(args[0], "!!") == 0) {
            if(strcmp(histline, "") == 0) {
                printf("No commands in history\n");
                continue;
            }
            printf("%s\n", histline);
            memcpy(args, parse_args(histline, ' '), (MAX_LINE/2 + 1) * sizeof(char *));
            memcpy(line, histline, MAX_LINE * sizeof(char));
        } else {
            memcpy(histline, line, MAX_LINE * sizeof(char));
        } 
        
        pid_t pid; //process id for forked child process
        int status;
        fflush(stdin);
        fflush(stdout);
        pid = fork(); //forking a child process
        int pipefd[2];
        pipe(pipefd);

        if(pid == 0) {
            //in child process

            //redirection, could be shortened
            int i, fd;
            i = get_file(args,"<");
            if(i >= 0) {
                //input
                fd = open(args[i], O_RDWR, 00777);
                args[i-1] = NULL; args[i] = NULL; //get rid of direction and file arguments
                if(fd < 0) {
                    printf("Error opening file: check if file exists\n");
                    break;
                }
                dup2(fd, 0);
                close(fd);
            }
            i = get_file(args, ">");
            if (i >= 0) {
                //output
                fd = open(args[i], O_RDWR | O_CREAT, 00777);
                args[i-1] = NULL; args[i] = NULL; //get rid of the direction and file arguments.
                if(fd < 0) {
                    printf("Error opening file");
                    break;
                }
                dup2(fd, 1);
                close(fd);
            }

            //piping
            char ** pargs; //pipe redirection arguments
            if(has_pipe(args) == 1) {
                pargs = parse_args(line, '|');
                char ** first_args = parse_args(pargs[0], ' ');
                args = parse_args(pargs[1],' '); //for the parent process.
                int pdpid = fork();
                if(pdpid == 0) {
                    //in child process, close read dup write
                    close(pipefd[0]);
                    dup2(pipefd[1], 1);
                    if(execvp(first_args[0], first_args) == -1) {
                        printf("First Execvp has failed, the error message is: %s\n", strerror(errno));
                    }
                    exit(0);
                } else {
                    //in parent process, close write dup read
                    close(pipefd[1]);
                    dup2(pipefd[0], 0);
                }
            }

            if(execvp(args[0], args) == -1) {
                printf("Execvp has failed, the error message is: %s\n", strerror(errno));
            }
            exit(0);
        } else if(pid > 0 && wait == -1){
            //in parent process
            //printf("waiting in parent process, pid is:%d ppid is: %d\n", getpid(), getppid());
            waitpid(pid, &status, 0);
        } else if(pid > 0 && wait != -1) {
            //reset wait flag
            //wait = -1;
            continue;
        } else {
            //fork has failed, returned -1
            printf("Fork has failed, the error message is: %s\n", strerror(errno));
        }

        //freeing memory
        while(*args != NULL) {
            free(*args);
            *args = NULL;
            *args++;
        }
        //free(args);
        free(line);

        //reset wait flag
        wait = -1;
   }
   free(histline);
   return 0;
}