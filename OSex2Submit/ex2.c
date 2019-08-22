/*
 * Author: Idan Twito
 * ID:     311125249
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdbool.h>

#define MAX_IN_LINE 512
#define MAX_COMMAND_ARGS 512
#define MAX_BACKGROUND_PROCESSES 512
#define SPACE_ASCII " "
#define PROMPT_SIGN "> "
#define NEW_LINE '\n'
#define EMPTY_CHAR '\0'
#define NOT_WAITING_SIGN "&"
#define EXIT_COMMAND "exit"
#define JOBS_COMMAND "jobs"
#define CD_COMMAND "cd"
#define HOME_PATH "~"
#define HOME_STRING "HOME"
#define EMPTY_STRING ""

typedef struct CommandStruct {
    int numOfArgs;
    char* commandLine;
    char** structCommandArgs;
    bool shouldWaitToSon;
    pid_t zombieId;
} CommandStruct;
/**
 * Function Name: zombiesCleaner
 * Function Input: CommandStruct* zombiesArr,int* zombiesArrIndex
 * Function Output: void
 * Function Operation: the function checks if are there any background processes
 *                     that ended, if so - delete them from memory and from jobs array.
 */
void zombiesCleaner(CommandStruct* zombiesArr,int* zombiesArrIndex){
    pid_t zombiePid;
    int status;
    int i=0;
    int j=0;
    int k=0;
    //-1 marks no zombie process found, otherwise stores the zombie process index
    int zombieRemovedIndex = -1;
    //if find zombie process - save the index of it in the jobs array
    while((zombiePid = waitpid(-1,&status,WNOHANG)) > 0){
        for(i = 0; i < (*zombiesArrIndex);i++){
            if(zombiesArr[i].zombieId == zombiePid){
                zombieRemovedIndex = i;
                break;
            }
        }
        //if found a process to delete from Jobs then delete all the memory of it.
        if(zombieRemovedIndex != -1) {
            zombiesArr[zombieRemovedIndex].zombieId = 0;
            zombiesArr[zombieRemovedIndex].shouldWaitToSon = false;
            for(j=0;j<zombiesArr[zombieRemovedIndex].numOfArgs;j++){
                zombiesArr[zombieRemovedIndex].structCommandArgs[j]="\0";
            }
            free(zombiesArr[zombieRemovedIndex].structCommandArgs);
            free(zombiesArr[zombieRemovedIndex].commandLine);
            zombiesArr[zombieRemovedIndex].numOfArgs = 0;
            for (k = zombieRemovedIndex; k < ((*zombiesArrIndex)-1); k++) {
                zombiesArr[k] = zombiesArr[k+1];
            }
            *zombiesArrIndex = ((*zombiesArrIndex) - 1);
        }
    }
}
/**
 * Function Name: commandLineRead
 * Function Input: None
 * Function Output: char* commandLine
 * Function Operation: the function gets a command line from the user, and
 *                     return it.
 */
char* commandLineRead(){
    char *commandLine;
    //max size of commd line
    size_t commandLineSize = MAX_IN_LINE;
    ssize_t charactersAmount;
    commandLine = (char*)malloc(commandLineSize* sizeof(char));
    if(commandLine == NULL){
        perror("An error occurred while trying malloc");
        return commandLine;
    }
    //gets the command line from user, charactersAmount gets the size of the command
    charactersAmount = getline(&commandLine,&commandLineSize,stdin);
    //remove the last \n that was entered via getline.
    if(commandLine[(charactersAmount-1)] == NEW_LINE){
        commandLine[(charactersAmount-1)] = EMPTY_CHAR;
    }
    return commandLine;
}
/**
 * Function Name: lineToArgs
 * Function Input: char* commandLine
 * Function Output: CommandStruct commandStruct
 * Function Operation: the function gets command line and creates a struct which
 *                     describes the command and necessary info about it such as
 *                     size of the command line, whether the command should run on
 *                     the background or not, the process id of the command.
 */
CommandStruct lineToArgs(char* commandLine){
    CommandStruct commandStruct;
    commandStruct.structCommandArgs = (char**)malloc(MAX_COMMAND_ARGS* sizeof(char*));
    //as default every command doesn't run on the background
    commandStruct.shouldWaitToSon = true;
    if(commandStruct.structCommandArgs == NULL){
        perror("An error occurred while trying malloc");
        return commandStruct;
    }
    char* oneWord;
    const char* spaceChar = SPACE_ASCII;
    int index = 0;
    //commandStruct.structCommandArgs will store each word in one member
    oneWord = strtok(commandLine,spaceChar);
    while(oneWord != NULL){
        commandStruct.structCommandArgs[index] = oneWord;
        index++;
        oneWord = strtok(NULL,spaceChar);
    }
    //removing the last index, which is not necessary
    commandStruct.structCommandArgs[index] = NULL;
    //if the last word is "&" - then mark this command as background command
    if(strcmp(commandStruct.structCommandArgs[(index-1)],NOT_WAITING_SIGN)  == 0){
        commandStruct.shouldWaitToSon = false;
        //remove the "&" from commandStruct.structCommandArgs
        commandStruct.structCommandArgs[(index-1)] = NULL;
        index--;
    }
    commandStruct.commandLine = commandLine;
    commandStruct.numOfArgs=index;
    return commandStruct;
}
/**
 * Function Name: simpleCommandExecute
 * Function Input: CommandStruct commandStruct, CommandStruct* zombiesArr
 *                 , int* zombiesArrIndex
 * Function Output: int value (1 or 0)
 * Function Operation: the function gets commandStruct which describes a
 *                     built-in command line, and then executes it.
 *                     the function knows to differentiate between background and
 *                     non-background commands, and executes them as desired.
 *                     then it returns: 1 if the command was executed with no issues.
 *                                      0 if the command faced any issue.
 */
int simpleCommandExecute(CommandStruct commandStruct
        ,CommandStruct* zombiesArr,int* zombiesArrIndex){
    pid_t pid;
    int status;
    pid = fork();
    if(pid == 0){
        //child: execute the command, if an error occurred then terminate the process.
        if(execvp(commandStruct.structCommandArgs[0],commandStruct.structCommandArgs) ==  -1){
            fprintf(stderr,"Error in system call\n");
            return 0;
        }
    }
    else if(pid != -1){
        //parent: if the child describes a background command then dont wait for
        //        it's implementation, otherwise - wait.
        printf("%d\n",pid);
        if((commandStruct.shouldWaitToSon)) {
            do {
                waitpid(pid, &status, WUNTRACED);
            }while((!WIFEXITED(status) && !WIFSIGNALED(status)));
        }//every background command - add it to jobs array.
        else{
            commandStruct.zombieId=pid;
            CommandStruct temp = commandStruct;
            zombiesArr[*zombiesArrIndex]=temp;
            *zombiesArrIndex = (*zombiesArrIndex) + 1;
        }
    }//if an error occurred in fork() print error.
    else{
        fprintf(stderr,"Error in system call\n");
    }
    return 1;
}
/**
 * Function Name: uniqueCommandExecute
 * Function Input: CommandStruct commandStruct, CommandStruct* zombiesArr,
 *                 int* zombiesArrIndex
 * Function Output: integer value (1 or 0)
 * Function Operation: the function gets commandStruct which describes one of the
 *                     following command lines: jobs, exit, cd.
 *                     then returns: 1 if the command was executed with no issues.
 *                                   0 if the command faced any issue.
 */
int uniqueCommandExecute(CommandStruct commandStruct,CommandStruct* zombiesArr,int*
zombiesArrIndex){
    if(strcmp(commandStruct.structCommandArgs[0], CD_COMMAND) == 0){
        //if the command is "cd" or "cd ~" then open Home Directory.
        if(commandStruct.structCommandArgs[1] == NULL ||
           strcmp(commandStruct.structCommandArgs[1],HOME_PATH) == 0){
            chdir(getenv(HOME_STRING));
        }
        //if an error ocured in chdir function.
        else if(chdir(commandStruct.structCommandArgs[1]) != 0){
            fprintf(stderr,"Error in system call\n");
        }
        printf("%d\n",getpid());
        return 1;
    }
    //if exit command is desired - then print process id and release all the
    // memory of the processes that haven't stopped yet.
    else if(strcmp(commandStruct.structCommandArgs[0], EXIT_COMMAND) == 0){
        int zombieIndex=0;
        int i=0;
        printf("%d\n",getpid());
        for(zombieIndex=0;zombieIndex<(*zombiesArrIndex);zombieIndex++){
            zombiesArr[zombieIndex].zombieId = 0;
            zombiesArr[zombieIndex].shouldWaitToSon = false;
            for(i=0;i<zombiesArr[zombieIndex].numOfArgs;i++){
                zombiesArr[zombieIndex].structCommandArgs[i]="\0";
            }
            free(zombiesArr[zombieIndex].structCommandArgs);
            free(zombiesArr[zombieIndex].commandLine);
            zombiesArr[zombieIndex].numOfArgs = 0;

        }
        return 0;
    }
    // if "jobs" command is desired - then print all the process ids and the names
    // of every command that is not finished
    else if(strcmp(commandStruct.structCommandArgs[0], JOBS_COMMAND) == 0){
        int k =0;
        int j = 0;
        for(k =0;k<(*zombiesArrIndex);k++) {
            printf("%d", zombiesArr[k].zombieId);
            for (j = 0; j < zombiesArr[k].numOfArgs; j++) {
                printf(" %s", zombiesArr[k].structCommandArgs[j]);
            }
            printf("\n");
        }
        return 1;
    }
    return 1;

}
/**
 * Function Name: commandExecute
 * Function Input: CommandStruct commandStruct, char** commandsArray
 *                 , int commandsArrayAmount,CommandStruct* zombiesArr
 *                 , int* zombiesArrIndex
 * Function Output: integer value (1 or 0)
 * Function Operation: the function gets commandStruct which describes a command
 *                     line and returns:
 *                                       1 if the command was executed with no issues.
 *                                       0 if the command faced any issue.
 */
int commandExecute(CommandStruct commandStruct,char** commandsArray,int commandsArrayAmount
        ,CommandStruct* zombiesArr,int* zombiesArrIndex){
    zombiesCleaner(zombiesArr,zombiesArrIndex);
    int i =0;
    //check if the command is: "jobs", "cd", or "exit" and implements it
    for(i = 0 ; i<commandsArrayAmount;i++){
        if(strcmp(commandStruct.structCommandArgs[0],commandsArray[i]) == 0)
            return uniqueCommandExecute(commandStruct,zombiesArr,zombiesArrIndex);
    }
    //else: implement as a normal built-in command.
    return simpleCommandExecute(commandStruct,zombiesArr,zombiesArrIndex);
}
/**
 * Function Name: shellLoop
 * Function Input: Void
 * Function Output: Void
 * Function Operation: The function gets a command line from the user and executes it (as
 *                     Shell does, by using assisting functions).
 */
void shellLoop(){
    char *line;
    int shouldRun = 1;
    CommandStruct commandStruct;
    char* commandsArray[] = {CD_COMMAND,EXIT_COMMAND
            ,JOBS_COMMAND};
    int commandsArrayAmount = sizeof(commandsArray)/ sizeof(char*);

    CommandStruct zombiesArr[MAX_BACKGROUND_PROCESSES]={0};
    int zombiesArrIndex = 0;
    while(shouldRun){
        printf(PROMPT_SIGN);
        line = commandLineRead();
        if(strcmp(line,EMPTY_STRING) == 0){
            continue;
        }
        commandStruct = lineToArgs(line);
        if(commandStruct.structCommandArgs == NULL){
            continue;
        }
        shouldRun = commandExecute(commandStruct,commandsArray,commandsArrayAmount
                ,zombiesArr,&zombiesArrIndex);
        if(commandStruct.shouldWaitToSon==true){
            free(line);
            free(commandStruct.structCommandArgs);
        }
    }
}
int main() {
    shellLoop();
    return 0;

}