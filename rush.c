/*
Violettee Muzondi
Rush Shell
This program prompts user to input command line arguements to emulate a unix shell. Exit, cd, and path are builtin
while other commands are found using their path to execute them. Redirection and parallel commands are executed as and
error messages are printed when errors are encountered.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/wait.h>
#include <fcntl.h>


#define INPUT_SIZE 255
char **g_path;
int num_path = 0;

// When error happens, print this message
void errorMessage(){
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
    fflush(stderr); 
}

// Take in user input and store it into line
char *read_line()
{
  char *line = NULL;
  size_t size = 0;
  ssize_t chars_read;
  chars_read = (getline(&line, &size, stdin) == -1);
  
  if (chars_read < 0){
    errorMessage();
    free(line);
    exit(1) ;
  }

  line[strcspn(line, "\n")] = '\0';

  return line;
}

// Split line into array args while taking out whitespace, return new array
char** split_line(char *input, int* wordcount){
    char *ch;
    int count = 0;
    char **arg = malloc(INPUT_SIZE * sizeof(char*));   // allocate space for char** array

    if (arg == NULL){
        printf("Malloc failed.");
        exit(1);
    }

    while ((ch = strsep(&input, " \t")) != NULL && count < INPUT_SIZE - 1) { //separate input based on delimitors 
        if (*ch != '\0' && !isspace(*ch)) {
            arg[count] = malloc(strlen(ch) + 1);
            strcpy(arg[count], ch);
            count++;
            (*wordcount)++;
        }
    }

    arg[count] = NULL;

    return arg;
}

// function that makes path when command is called
void create_path(char* newPath) {
    g_path = realloc(g_path, (num_path + 1) * sizeof(char*)); //allocate space for global path array    
    g_path[num_path] = strdup(newPath);
    num_path++;
}


// function to redirect input to file
void redirect(char **current_cmd, int numArg, int* redirec){
    // Check for ">"
    for (int i = 0; current_cmd[i] != NULL; ++i) {
        if (strcmp(current_cmd[i], ">") == 0) {
            if (current_cmd[i + 1] != NULL && current_cmd[i + 2] == NULL) {
                // Redirect input a file
                int fd = open(current_cmd[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    errorMessage();
                    exit(1);
                }

                printf("Redirecting output to file: %s\n", current_cmd[i + 1]);  
                dup2(fd, STDOUT_FILENO);
                close(fd);

                current_cmd[i] = NULL;  
                current_cmd[i + 1] = NULL;  
                break;  


            } else {
                errorMessage();
                exit(1);
            }
        }
    }
}


// function to separate commands into char** array
void split_cmds(char** args, int numArg, int* commandGroups, char* commandList[50][50]) {
    int cmd_i = 0;
    int arg_i = 0;

    for (int i = 0; i < numArg; i++) {
        if (strcmp(args[i], "&") == 0) {            // eliminate "&" 
            commandList[cmd_i][arg_i] = NULL; 
            cmd_i++;
            arg_i = 0;
        }else {
            commandList[cmd_i][arg_i] = args[i];
            arg_i++;
        }
    }
    commandList[cmd_i][arg_i] = NULL;               // make end of list empty
    *commandGroups = cmd_i + 1;
}


// See if args contains builtin commands, and execute them
int check_builtins(char **current_cmd, int numArg){
    if (strcmp(current_cmd[0], "exit") == 0) {             // if first command is "exit", exit program 
        if (numArg > 1) {
            errorMessage();                     
            return 1;
        }else{
            exit(0);;
        }
    }
    if (strcmp(current_cmd[0], "cd") == 0) {               // if first command is "cd", change directory
        if (numArg == 2) {
            if (chdir(current_cmd[1]) != 0) {
                errorMessage();
            }
        }else{
            errorMessage();
        }
        return 1;
    }
    if (strcmp(current_cmd[0], "path") == 0) {             // if first command is "path" with arguements, add path
        for (int j = 0; j < num_path; j++) {
            free(g_path[j]);
        }
            
        free(g_path);
        g_path = NULL;
        num_path = 0;
        for (int j = 1; j < numArg; j++) {
            create_path(current_cmd[j]);
        }
        return 1;
    }
    return 0;    
    }
    


/* Main function with:
    - built in commands: exit, cd, & path
    - path directory "/bin"
    - fork()
    - redirection
    - parallel commands
*/
int main(int argc, char* argv[]){
    // If there is more than one argument when calling ./rush, print error & exit
    if (argc > 1) {
        errorMessage();
        exit(1);
    }

    // initialize bin directory
    g_path = NULL;
    create_path("/bin");
    

    // loop to prompt user for input until error or exit
    while(1) {
        //initialize the variables storing command line arguements
        int wordcount = 0;
        int numCmd = 0;

        printf("rush> ");
        fflush(stdout);
        char *line = read_line();                             // read user input & store into line
        char **args = split_line(line, &wordcount);                        // splice line into array of strings, args
        free(line);
        
        char* cmdArray[50][50]; 
        split_cmds(args, wordcount, &numCmd, cmdArray); // separate commands into char** array  
        int child_pid[numCmd];                          // create pids based on number of commands

        for (int i = 0; i < numCmd; i++) {
            char** current_cmd = cmdArray[i];           // if empty command put in, prompt rush
            if (current_cmd[0] == NULL) {
                continue; 
            }
            int numArg = 0;                              
            while (current_cmd[numArg] != NULL){            // find the number of arguments
                numArg++;                                   
            }
            int status = check_builtins(current_cmd, numArg);
            if(status == 1){                                // prompt rush
                continue;
            }
            int cmdFound = 0;
            for (int j = 0; j < num_path; j++) {        // find the index of the commands
                char complete_path[256];
                snprintf(complete_path, sizeof(complete_path), "%s/%s", g_path[j], current_cmd[0]);
                
                if (access(complete_path, X_OK) == 0) {
                    cmdFound = 1;
                    child_pid[i] = fork();

                    if (child_pid[i] == 0) {            // if the process if the child, then check if there's redirection
                        int redirec = 0;
                        redirect(current_cmd, numArg, &redirec);
                        if (redirec) {                  // if redirection is true, execute redirection process
                            exit(1);
                        }
                        execv(complete_path, current_cmd);  // execute commands
                        exit(1);                        //if execv fails, exit
                    }
                    break;
                }
            }
            if (cmdFound == 0) {                        //if the command is not found then print error message
                errorMessage();
            }
        }

    for (int i = 0; i < numCmd; i++) {              // wait for each child process to finish
        if (child_pid[i] > 0) {
            wait(NULL);
        }
    }

    for (int i = 0; i < wordcount; i++) {
        free(args[i]);                             //iterate through word array and free each one
    }
    free(args);
    }

    for (int i = 0; i < num_path; i++) {
        free(g_path[i]);
    }
    free(g_path);
    return 0;
}
