#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wait.h>
#include <dirent.h>

#define MAX_LENGTH 1024
#define MAX_ARGS 16

void removeAffix(char *str) {
    //removes prefix 'PATH=' and 'HOME=' and suffix '\n'
    size_t len = strlen(str);
    if (str[len - 1] == '\n') {
        memmove(str, str + 5, len - 6);
        str[len - 6] = '\0';
    } else {
        memmove(str, str + 5, len - 5);
        str[len - 5] = '\0';
    }
}

int runProgram(char *path, char **args) {
    pid_t pid, wpid;
    int status, success = 0;

    pid = fork();
    if (pid == 0) {
        // Child process
        char *programPath = malloc(sizeof(char) * MAX_LENGTH);
        strcat(programPath, path);
        strcat(programPath, "/");
        strcat(programPath, args[0]);

        if (execv(programPath, args) == -1) {
            printf("Error running process %s\n", args[0]);
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        printf("Error forking\n");
    } else {
        // Parent process
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
            success = 1;
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return success;
}

int numQuotes(char *str) {
    // return number of qoute characters of a certain character in a string and remove them from the string
    int num = 0, i = 0;
    while (str[i] != '\0') {
        if (str[i] == '\"') {
            memmove(&str[i], &str[i + 1], strlen(str) - i);
            num++;
        } else {
            i++;
        }
    }
    return num;
}

int main() {
    // get current directory
    char dir[MAX_LENGTH];
    getcwd(dir, sizeof(dir));

    // read profile file
    FILE *ptr_profile = fopen("profile", "r");
    if (!ptr_profile) {
        printf("File 'profile' does not exist in current directory\n");
        return 0;
    }

    // get PATH and HOME
    char pathstr[MAX_LENGTH], home[MAX_LENGTH], line1[MAX_LENGTH], line2[MAX_LENGTH], prefix1[5], prefix2[5];
    fgets(line1, MAX_LENGTH, ptr_profile);
    fgets(line2, MAX_LENGTH, ptr_profile);
    strncpy(prefix1, line1, 5);
    strncpy(prefix2, line2, 5);
    if (!strcmp(prefix1, "PATH=")) {
        strcpy(pathstr, line1);
        if (!strcmp(prefix2, "HOME=")) {
            strcpy(home, line2);
        } else {
            printf("Invalid profile specification: 'HOME=' does not exist\n");
            return 0;
        }
    } else if (!strcmp(prefix1, "HOME=")) {
        strcpy(home, line1);
        if (!strcmp(prefix2, "PATH=")) {
            strcpy(pathstr, line2);
        } else {
            printf("Invalid profile specification: 'PATH=' does not exist\n");
            return 0;
        }
    } else {
        printf("Invalid profile specification: Each line must start with 'PATH=' or 'HOME='\n");
        return 0;
    }
    removeAffix(pathstr);
    removeAffix(home);

    // convert path to array of strings
    char **path = malloc(sizeof(char *) * MAX_LENGTH);
    char *pathToken = strtok(pathstr, ":");
    int i = 0;
    while (pathToken != NULL) {
        path[i] = malloc(sizeof(char) * strlen(pathToken));
        strcpy(path[i], pathToken);
        pathToken = strtok(NULL, ":");
        i++;
    }

    // main loop
    while (1) {
        printf("%s> ", dir);

        // read input
        char *input = NULL;
        size_t bufsize = 0;
        getline(&input, &bufsize, stdin);

        // get arguments
        char **args = malloc(sizeof(char *) * MAX_ARGS);
        char *inToken = strtok(input, " \n");
        i = 0;
        int validArgs = 1;
        while (inToken != NULL) {
            char *toCopy = malloc(sizeof(char) * MAX_LENGTH);

            int quotes = numQuotes(inToken);
            strcpy(toCopy, inToken);
            while (quotes % 2) {
                inToken = strtok(NULL, " \n");
                if (inToken == NULL) {
                    if (quotes % 2) {
                        validArgs = 0;
                        printf("Invalid arguments\n");
                    }
                    break;
                }
                quotes += numQuotes(inToken);
                strcat(toCopy, " ");
                strcat(toCopy, inToken);
            }
            if (!validArgs)
                break;

            args[i] = malloc(sizeof(char) * MAX_LENGTH);
            strcpy(args[i], toCopy);
            inToken = strtok(NULL, " \n");
            i++;
        }
        if (!validArgs)
            continue;

        // reassignment of $HOME
        if (!strncmp(args[0], "$HOME=", 6)) {
            if (opendir(args[0] + 6))
                strcpy(home, args[0] + 6);
            else
                printf("Invalid directory %s\n", args[0] + 6);
            continue;
        }

        // reassignment of $PATH
        if (!strncmp(args[0], "$PATH=", 6)) {
            char *newPathStr = args[0] + 6;
            char **newPath = malloc(sizeof(char *) * MAX_LENGTH);
            char *newPathToken = strtok(newPathStr, ":");

            i = 0;
            int valid = 1;
            while (newPathToken != NULL) {
                if (!opendir(newPathToken)) {
                    printf("Invalid directory %s\n", newPathToken);
                    valid = 0;
                    break;
                }
                newPath[i] = malloc(sizeof(char) * strlen(newPathToken));
                strcpy(newPath[i], newPathToken);
                newPathToken = strtok(NULL, ":");
                i++;
            }

            if (valid)
                path = newPath;
            continue;
        }

        // If an argument(s) starts with ~ change it to the value for home
        i = 0;
        while (args[i] != NULL) {
            if (args[i][0] == '~') {
                if (args[i][1] == '\0') {
                    strcpy(args[i], home);
                } else if (args[i][1] == ':' || args[i][1] == '/') {
                    char *temp = malloc(sizeof(char) * strlen(args[i]));
                    strcpy(temp, args[i] + 1);
                    strcpy(args[i], home);
                    strcat(args[i], temp);
                }
            }
            i++;
        }

        // exit shell
        if (!strcmp(args[0], "exit")) {
            break;
        }

        // cd
        if (!strcmp(args[0], "cd")) {
            char *newdir = malloc(sizeof(char) * MAX_LENGTH);

            if (args[1] == NULL) {
                strcpy(newdir, home);
            } else if (!strcmp(args[1], ".")) {
                continue; // do nothing
            } else if (!strcmp(args[1], "..")) {
                // get parent directory
                if (!strcmp(dir, "/"))  // if at root do nothing
                    continue;
                i = 0;
                while (dir[i] != '\0')
                    i++;
                while (dir[i] != '/')
                    i--;
                int j = 0;
                while (j != i) {
                    newdir[j] = dir[j];
                    j++;
                }
                if (!strcmp(newdir, ""))
                    newdir = "/";
            } else if (args[1][0] == '/') {
                strcpy(newdir, "/");
                if (args[1][1] != '\0')
                    strcat(newdir, args[1] + 1);
            } else {
                strcpy(newdir, dir);
                strcat(newdir, "/");
                strcat(newdir, args[1]);
                if (!strcmp(dir, "/"))
                    newdir++;
            }

            if (chdir(newdir) != 0) {
                printf("Invalid directory %s\n", newdir);
            } else {
                strcpy(dir, newdir);
            }
            continue;
        }

        // find the program in path and run it
        int programExists = 0;
        i = 0;
        while (path[i] != NULL) {
            chdir(path[i]);
            if (access(args[0], F_OK) != -1) {
                // program exists
                chdir(dir);
                runProgram(path[i], args);
                programExists = 1;
                break;
            }
            i++;
        }
        chdir(dir);

        // program not found
        if (!programExists) {
            printf("%s: command not found\n", args[0]);
        }
    }

    return 1;
}