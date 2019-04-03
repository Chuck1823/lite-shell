#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "parse.h"   /*include declarations for parse-related structs*/

/* Constants used for feature 6, the history command that keeps track of the 30 (extra) latest previously issued commands. */
#define MAX_CMD_LEN 128
#define HISTORY_COUNT 30

/* Added cd, kill, help, ! and history to the built-in commands enum. */
enum BUILTIN_COMMANDS {NO_SUCH_BUILTIN=0, EXIT, JOBS, HELP, KILL, CD, HISTORY, REPEAT};

/*Feature 1 (plus extra) implemented in the buildPrompt() method using getcwd() and gethostname() system calls*/
 
char *buildPrompt()
{
  char cmd[1024];
  getcwd(cmd, 1024);
  printf("CWD:%s ", cmd);
  gethostname(cmd, 1024);
  printf("HOST NAME: %s ", cmd);
  return  "%";
}

/* Struct needed for the 'jobs' built-in command */
typedef struct bgNode {
  int pid;
  char *command;
  struct bgNode * next;
} bgNode_t;

/* Feature 6 (plus extra) defined by the history function. This function takes in an char array of previously issued commands plus an integer specifying the current command. It simply loops through the array, printing the oldest to the latest command. */

int history(char *hist[], int current) {
  int i = current;
  int hist_num = 1;

  do {
    if(hist[i]) {
      printf("%4d %s\n", hist_num, hist[i]);
      hist_num++;
    }
    
    i = (i + 1) % HISTORY_COUNT;
  } while (i != current);

  return 0;
}

/* Modified isBuiltInCommand to accomodate the jobs, cd, kill, help, ! and history built-in commands*/

int isBuiltInCommand(char * cmd){
  if(strncmp(cmd, "exit", strlen("exit")) == 0) {
      return EXIT;
  } else if(strncmp(cmd, "jobs", strlen("jobs")) == 0) {
      return JOBS;
  } else if(strncmp(cmd, "help", strlen("help")) == 0) {
      return HELP; 
  } else if(strncmp(cmd, "kill", strlen("kill")) == 0) {
      return KILL;
  } else if(strncmp(cmd, "cd", strlen("cd")) == 0) {
      return CD;
  } else if(strncmp(cmd, "history", strlen("history")) == 0) {
      return HISTORY;
  } else if(strncmp(cmd, "!", strlen("!")) == 0) {
      return REPEAT;
  }
  return NO_SUCH_BUILTIN; 
}


int main (int argc, char **argv)
{
  /* Integer used to store the child process id. */
  int childPid;

  /* Integer used to call waitpid that needs a integer variable pointer to store the exit status of the child process */
  int status;

  /* Data structures to store the number and index of a command that needs to be repeated with '!' */
  char *num;
  int index;

  /* Data structures needed for feature 4, the redirection of STDIN and STDOUT. Two FILE type variable pointers called inputfil and outputfile are needed. */
  FILE *inputfile, *outputfile;

  /* Initialization of the structures necessary for feature 6, the history feature that keeps track of the 30 latest previously issued commands. */
  char cmd[MAX_CMD_LEN];
  char *hist[HISTORY_COUNT];
  int i, current = 0;

  /* Variable used to keep track of the process id to kill. */
  char *killPid;

  char * cmdLine;
  parseInfo *info; /*info stores all the information returned by parser.*/
  struct commandType *com; /*com stores command name and Arg list for one command.*/
  
  /* Initialization of structs pointers needed for the 'jobs' built-in command */
  bgNode_t *head = NULL;
  bgNode_t *curr = NULL;
  bgNode_t *temp = NULL;
  
  /* Initializes all entries of the hist char array to NULL. */
  for(i = 0; i < HISTORY_COUNT; i++) {
    hist[i] = NULL;
  }

#ifdef UNIX  
    fprintf(stdout, "This is the UNIX version\n");
#endif

#ifdef WINDOWS
    fprintf(stdout, "This is the WINDOWS version\n");
#endif

  while(1){

#ifdef UNIX
    /* The line below serves to print the prompt as long as we are in the while(1) loop*/
    cmdLine = readline(buildPrompt());
    if (cmdLine == NULL) {
      fprintf(stderr, "Unable to read command\n");
      continue;
    }
#endif    

    /*calls the parser*/
    info = parse(cmdLine);
    if (info == NULL){
      free(cmdLine);
      continue;
    }

    /*com contains the info. of the command before the first "|"*/
    com=&info->CommArray[0];
    if ((com == NULL)  || (com->command == NULL)) {
      free_info(info);
      free(cmdLine);
      continue;
    }

    /* Feature 7 where if the user enters '!' followed by the number of a previously issued command from the list produced by the history command, that command gets executed again. This calls the parser again, this time with the selected command from the hist[] array.*/
    /* com then gets reset and we fall through the rest of the procedure. Note that that selected command is also not added to history for redundancy purposes. */
    if(isBuiltInCommand(com->command) == REPEAT) {
      num = (com->command) + 1;
      index = atoi(num);
      if(index > 0) {
        index = index - 1;
        if(hist[index] != NULL) {
          info = parse(hist[index]);
          com = info->CommArray;
        } else {
          printf("No command in history for the number specified..\n");
        }
      } else if(index < 0) {
        index = index + current;
        if(hist[index] != NULL) {
          info = parse(hist[index]);
          com = info->CommArray;
        } else {
          printf("No command in history for the number specified..\n");
        }
      } else if(index == 0) {
        printf("Entry not valid!\n");
      }
    } else {
      /* Populate the hist char array as commands are issued while updating the 'current' pointer. */
      if(hist[current]) free(hist[current]);
      hist[current] = (char *) strdup(cmdLine);
      current = (current + 1) % HISTORY_COUNT;
    }

    /* Not a built-in command */
    if(!isBuiltInCommand(com->command)) {
      childPid = fork();
      if(childPid == 0) {
        /* Feature 4 allowing the user to redirect STDIN and STDOUT for new processes. The parser takes care of '<' and '>' and instead sets a flag in the info struct for us to use the proper procedure. */
        if(info->boolInfile) {
          inputfile = fopen(info->inFile, "r");
          dup2(fileno(inputfile), 0);
        }
        if(info->boolOutfile) {
          outputfile = fopen(info->outFile, "w");
          dup2(fileno(outputfile), 1);
        }
        /* Feature 2 allowing the user of the shell to execute commands by using the relative or absolute pathnames for them using the execvp system call. */
        execvp(com->command, com->VarList);
        exit(0);
      } else {
        if(info->boolBackground) {
          temp = malloc(sizeof(bgNode_t));
          temp->pid = childPid;
          temp->command = (char *) strdup(com->command);
          temp->next = head;
          head = temp;
        } else {
          waitpid(childPid, &status, 0);
        }
      }
    /* Exit built-in command */
    } else if(isBuiltInCommand(com->command) == EXIT) {
        /* Feature 9 where the user is instructed to kill processes running in the background before being able to exit the shell. If the head of the background processes linked list is NULL (meaning there are none), exit immediately */
        if(head == NULL) {
          exit(1);
        } else {
          /* If there are background processes running, check to see if they are all done */
          curr = head;
          temp = head;
          while(curr != NULL) {
            /* Checks to see if the current process if finished and updates pointers accordingly */
            if(curr->pid == waitpid(curr->pid, NULL, WNOHANG)) {
              if(curr == head) {
                head = head->next;
                curr = head;
                temp = head;
              } else {
                temp->next = curr->next;
                temp = curr;
                curr = curr->next;
                free(temp->command);
                free(temp);
                temp = curr;
              }
            } else {
              temp = curr;
              curr = temp->next;
            }
          }
         }
         
         /* Once the linked list of background processes has been updated, re-check to see if there are any more running ones. If there are, notify the user, else exit */
         if(head == NULL) {
           exit(1);
         } else {
           printf("There are existing background processes still running! Please kill these before proceeding to exit.\n");
         }
    /* Jobs built-in command */
    } else if(isBuiltInCommand(com->command) == JOBS) {
        /* No background jobs currently running */
        if(head == NULL) {
          printf("No background jobs.\n");
        } else {
          curr = head;
          temp = head;
          while(curr != NULL) {
            /* Checks to see if the current process is finished and updates pointers accordingly */
            if(curr->pid == waitpid(curr->pid, NULL, WNOHANG)) {
              if(curr == head) {
                head = head->next;
                curr = head;
                temp = head;
              } else {
                temp->next = curr->next;
                temp = curr;
                curr = curr->next;
                free(temp->command);
                free(temp);
                temp = curr;
              }
            } else {
              /* Print unfinished process and update pointers */
              printf("Process: %d\t%s\n", curr->pid, curr->command);
              temp = curr;
              curr = temp->next;
            }
          }
        }
    /* Cd built-in command */
    } else if(isBuiltInCommand(com->command) == CD) {
        /* If the feature 8 built-in command 'cd' is issued, the current working directory is changed by using the chdir system call. chdir() takes in the path to change the cwd to which is the second argument in the VarList[] of the com struct. */
        chdir(com->VarList[1]);
    /* History built-in command */
    } else if(isBuiltInCommand(com->command) == HISTORY) {
        printf("List of commands previously issued.\n");
        /* If the history command is issued, the feature 6 function called 'history' will be called here. */
        history(hist,current);
    /* Kill built-in command */
    } else if(isBuiltInCommand(com->command) == KILL) {
        /* Convert the process id to an integer */
        killPid = com->VarList[1];
        killPid++;
        /* If the kill command is issued, the process whose pid is killPid will be killed using the kill system call combined with SIGKILL for a signal. If the killPid given does not exist in the list of currently running background jobs, nothing will happen */
        kill(atoi(killPid), SIGKILL);
    /* Help built-in command */
    } else if(isBuiltInCommand(com->command) == HELP) {
        printf("Available built-in commands\n");
        printf("EXIT: Terminates the shell process. No arguments needed.\n");
        printf("CD: Change the current working directory. Argument should indicate relative path.\n");
        printf("HISTORY: Print the list (latest 30) of previsouly executed commands. No arguments needed. The list will be ordered such that each command number can be combined with '!' (in the !# format) to indicate the command to repeat. If the # following the ! is negative, -1 signifies the latest command will be repeated, -2 the second to last command, etc. A # of 0 will be considered an invalid input. \n");
        printf("KILL: Terminate the process numbered in the list of background processes returned by jobs. The 'kill' command must be followed by the percent symbol and a number  where the number is the process id number found using the command jobs.\n");
        printf("HELP: Lists the shell's available built-in commands and their syntax.\n");
    }

    /* Feature 8 implemented with the above 'else if' statements that 'catch' all of the required built-in commands. These commands do not create a new process but instead use system calls with the functionality 'built-in' the shell. */ 
    
    free_info(info);
    free(cmdLine);
  }
}
  






