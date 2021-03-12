#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h> 
#include <sys/types.h>


extern int errno ;
#define WRITE_FD 104

//arguments in a command
struct arguments
{
    char* argument;
    struct arguments* next;
};
//contains atomic commands in a command with redirection using pipes
struct simple_command{
    char* binaryFile;
    struct arguments* arguments;
    int num_args;
    struct simple_command* next_simple;
};

struct command
{
    /* data */
    struct simple_command* basic;   //in case there is redirection with pipes
    int pid;
    int background;
    int current;
    int num_args;
    int command_index;
    char* binaryFile;
    struct arguments* arguments;
};

//list of commands
struct commands
{
    struct command det_com;
    struct commands *command_next;
};


//list of current processes;
struct current_processes{
    int pid;
    char* binaryFile;
    struct current_processes *next_proc;
};

char* bufferOutput = NULL;

int parent_pid;
char *hostname,*username,*pwd;
struct commands* head_c = NULL;
struct current_processes* curr_proc_h = NULL;
char* command_shell;

void handler(int sig);
//Handler functions for list of current processes
void deleteCurrentProc(struct current_processes** curr_proc_head,int pid); //remove the background process's pid (when the background process signals the parent that its finished) from the list of currently executing processes
void printCurrentProc(struct current_processes* curr_proc_h); //prints all the currently executing process- used in $pid current
void addCurrentProc(struct current_processes** curr_proc_head,int pid,char* binaryFile); //add a backgroud process details when a background starts executing

struct command Search(struct commands* head,int mode,int num,char* name,int pid);   //search from all processes executed based on PID, Order Number and Name
//Handler functions for arguments of a command
void PrintArguments(struct arguments* head);
void addArgument(struct arguments** head,char* arg);
//Handler functions for list of commands
void addCommand(struct commands** head_,struct command add);
void PrintHis(struct commands* head,int total_commands,int N);
//Handler functions for basic commands in a command using pipes
void AddSimpleCommand(struct simple_command** head,struct simple_command add);
void PrintSimpleCommand(struct simple_command* head);
int Exec_simple_commands(struct simple_command* execute_simple, int max_size_command,int pipes);

int Exec(struct command execute,int max_size_command,int background);
void command_prompt(struct commands** head_c,char* prog_name);

char* extract_display_info(int mode);
void change_dir();  //for change directory functionality
void sigproc();

int main(int argc,char *argv[]) {
    
    if(argc > 1) {
        printf("usage: ./prog_name\n");
        return -1;
    }

    int parent_pid = getpid();
    hostname = extract_display_info(1);
    username = extract_display_info(2);
    pwd = extract_display_info(3);
    int total_len = strlen(hostname) + strlen(username) + strlen(pwd);
    command_shell = (char*) malloc((total_len+1+4)*sizeof(char));
    if (command_shell) {
        memcpy(command_shell,"<",1);
        memcpy(command_shell + 1 ,username, strlen(username));
        memcpy(command_shell + 1 + strlen(username),"@",1);
        memcpy(command_shell + 2 +  strlen(username), hostname, strlen(hostname));
        memcpy(command_shell + 2 + strlen(hostname) + strlen(username), ":",1);
        memcpy(command_shell + 3 + strlen(hostname) + strlen(username), pwd, strlen(pwd));
        memcpy(command_shell + 3 + strlen(hostname) + strlen(username) + strlen(pwd), ">",2);
    }
    // printf("\n%s\n",command_shell);
    signal(SIGINT, sigproc);
    
    command_prompt(&head_c,argv[0]);
    // cli(head_c,max_size_command,total_commands);
    return 0;
}

void handler(int sig)
{

    pid_t p;
    int status;

    if ((p=waitpid(-1, &status, WNOHANG)) != -1)
    {
       /* Handle the death of pid p */
       if (p!=0) {
            if (p != -1 && WIFEXITED(status))
            {
                int estat = WEXITSTATUS(status);
                struct command comp = Search(head_c,3,-1,"\0",p);
                char *err = (estat == 0) ? "success" : "failure";
                // printf("--- executed with PID %d exited with status %d (%s)----\n", p, estat, err);
                printf("--- %s executed with PID %d exited with status %d----\n",comp.binaryFile, p, estat);
                deleteCurrentProc(&curr_proc_h,p);
            }
        //    printf("\nPid %d exited with status %d.", p, status);
       }
    }
}

void deleteCurrentProc(struct current_processes** curr_proc_head,int pid){
    // printf("Deleting this process %d\n",pid);
    struct current_processes* curr_proc = *curr_proc_head;
    if (curr_proc->pid == pid){
        *curr_proc_head = (*curr_proc_head)->next_proc;
        return;
    }
    while(curr_proc->next_proc->pid == pid){
        curr_proc = curr_proc->next_proc;
    }
    if (curr_proc!=NULL) curr_proc->next_proc = curr_proc->next_proc->next_proc;
}

void addCurrentProc(struct current_processes** curr_proc_head,int pid,char* binaryFile){
    // printf("Adding current process with pid %d",pid);
    struct current_processes* new_curr_proc = (struct current_processes*) malloc(sizeof(struct current_processes));
    new_curr_proc->pid = pid;
    new_curr_proc->binaryFile = binaryFile;
    new_curr_proc->next_proc = NULL;
    if (*curr_proc_head == NULL){
        *curr_proc_head = new_curr_proc;
        return;
    }
    struct current_processes* curr_proc = *curr_proc_head;
    while(curr_proc->next_proc!=NULL) {
        curr_proc = curr_proc->next_proc;
    }
    curr_proc->next_proc = new_curr_proc;
}


void printCurrentProc(struct current_processes* curr_proc_h){
    // printf("Printing current processes");
    while(curr_proc_h!=NULL) {
        printf("command name: %s \t\t process id: %d",curr_proc_h->binaryFile,curr_proc_h->pid);
        if (curr_proc_h->next_proc!=NULL) printf("\n");
        curr_proc_h = curr_proc_h->next_proc;
    }
    printf("\n");
}

void PrintArguments(struct arguments* head) {
    // printf("\nprinting args");
    while(head!=NULL) {
        printf("%s ",head->argument);
        head = head->next;
    }
}

void PrintCommands(struct commands* head) {
    // printf("\nprinting commands");
    while(head!=NULL) {
        // printf("%d. ",head->det_com.command_index);
        // PrintArguments(head->det_com.arguments);
        // printf(" The num of args: %d",head->det_com.num_args);
        printf("command name: %s  \t\t process id: %d",head->det_com.binaryFile,head->det_com.pid);
        if (head->command_next!=NULL) printf("\n");
        // printf(" PID:%d Background:%d Current:%d\n",head->det_com.pid,head->det_com.background,head->det_com.current);
        head = head->command_next;
    }
    printf("\n");
}

void addCommand(struct commands** head_,struct command add){
    // PrintArguments(add.arguments);
    // PrintCommands(*head_);
    struct commands* new_command =  (struct commands*)malloc(sizeof(struct commands));
    new_command->det_com.binaryFile = add.binaryFile;
    new_command->det_com.arguments = add.arguments;
    new_command->det_com.num_args = add.num_args;
    new_command->det_com.pid = add.pid;
    new_command->det_com.background = add.background;
    new_command->det_com.command_index = add.command_index;
    new_command->det_com.basic = add.basic;
    new_command->command_next = NULL;
    if (*head_==NULL) {
        *head_ = new_command;
        return;
    }
    struct commands* curr = *head_;
    while(curr->command_next!=NULL) {
        curr = curr->command_next;
    }
    curr->command_next = malloc(sizeof(struct commands));
    curr->command_next->det_com.binaryFile = add.binaryFile;
    curr->command_next->det_com.arguments = add.arguments;
    curr->command_next->det_com.num_args = add.num_args;
    curr->command_next->det_com.command_index = add.command_index;
    curr->command_next->det_com.pid = add.pid;
    curr->command_next->det_com.background = add.background;
    curr->command_next->det_com.basic = add.basic;
    curr->command_next->command_next = NULL;
    // PrintCommands(*head_);
    // return head;
}


void addArgument(struct arguments** head,char* arg){
    struct arguments* new_arg = (struct arguments*)malloc(sizeof(struct arguments));
    new_arg->argument = arg;
    new_arg->next = NULL;
    if (*head == NULL) {
        *head = new_arg;
        return;
    }
    struct arguments* curr = *head;
    while(curr->next!=NULL) {
        curr = curr->next;
    }
    curr->next = (struct arguments*)malloc(sizeof(struct arguments));
    curr->next->argument = arg;
    curr->next->next = NULL;
}

void PrintHis(struct commands* head,int total_commands,int N){
    int i = 0;
    if (N > total_commands) i = 1;
    while(head!=NULL) {
        if (i>=(total_commands-N)){
            printf("%d. %s",head->det_com.command_index,head->det_com.binaryFile);
            // PrintArguments(head->det_com.arguments);
            printf("\n");
        }
        head = head->command_next;
        i = i + 1;
    }
    printf("\n");
}

struct command Search(struct commands* head,int mode,int num,char* name,int pid){
    while(head!=NULL){
        if (head->det_com.command_index==num && mode==1){
            return head->det_com;
        } else if (strcmp(head->det_com.binaryFile,name)==0 && mode==2) {
            return head->det_com;
        } else if (head->det_com.pid == pid && mode==3){
            return head->det_com;
        }
        head = head->command_next;
    }
    struct command empty;
    empty.command_index = -1;
    empty.binaryFile = NULL;
    empty.arguments = NULL;
    return empty;
} 

void AddSimpleCommand(struct simple_command** head_,struct simple_command add) {
    struct simple_command* new_command =  (struct simple_command*)malloc(sizeof(struct simple_command));
    new_command->binaryFile = add.binaryFile;
    new_command->arguments = add.arguments;
    new_command->num_args = add.num_args;
    new_command->next_simple = NULL;
    if (*head_==NULL) {
        *head_ = new_command;
        return;
    }
    struct simple_command* curr = *head_;
    while(curr->next_simple!=NULL) {
        curr = curr->next_simple;
    }
    curr->next_simple = malloc(sizeof(struct simple_command));
    curr->next_simple->binaryFile = add.binaryFile;
    curr->next_simple->arguments = add.arguments;
    curr->next_simple->num_args = add.num_args;
    curr->next_simple->next_simple= NULL;
    // PrintCommands(*head_);
    // return head;
}

void PrintSimpleCommand(struct simple_command* head) {
    while(head!=NULL) {
        PrintArguments(head->arguments);
        printf("command name: %s  \t\t Number of arguments: %d",head->binaryFile,head->num_args);
        if (head->next_simple!=NULL) printf("\n");
        // printf(" PID:%d Background:%d Current:%d\n",head->det_com.pid,head->det_com.background,head->det_com.current);
        head = head->next_simple;
    }
    printf("\n");
}

void SimpleExecution(struct arguments* arguments_list,int max_size_command,int num_args,int pipe_number,int islast) {
    // PrintArguments(arguments_list);
    struct arguments* arguments_head = arguments_list;
    char** args = malloc(sizeof((num_args+1)*sizeof(char*)));
    int input_re = 0;
    int output_re = 0;
    char* output_file = (char *)malloc(max_size_command+1);
    char* input_file = (char* )malloc(max_size_command+1);
    int filedes[2]; // pos. 0 output, pos. 1 input of the pipe
    int filedes2[2];

    for (int j = 0;j<num_args+1;j++){
        args[j] = (char *)malloc(max_size_command+1);
    }
    int i = 0, k = 0;
    for (i=0;i<num_args;i++){
        if ((strcmp(arguments_list->argument,"<") == 0 ) || (strcmp(arguments_list->argument,">") == 0 ) ) {    //i/o redirection
            if (strcmp(arguments_list->argument,">") == 0 ) {   //output redirection
                output_file = arguments_list->next->argument; 
                output_re = 1;
            } else {
                input_file = arguments_list->next->argument;    //input redirecton
                input_re = 1;
            }
            if (arguments_list->next->next == NULL) break;  //if no file is provided break;
            arguments_list = arguments_list->next->next;
            continue;
        }
        args[k] = arguments_list->argument;
        arguments_list = arguments_list->next;
        k++;
    }
    // printf("%s ",args[k]);
    // PrintCommands(chain_commands);
    args[k] = NULL;


    if (pipe_number % 2 != 0){
        pipe(filedes); // for odd pipe_number
    }else{
        pipe(filedes2); // for even pipe_number
    }

    int child = fork();
    if (child < 0) {
        printf("Fork failed");
        return;
    }

    if (child==0) {
        // printf("Children execution started\n")


        if (pipe_number == 0){  //if first commmand in the sequence then we only add pipe to its write end only (provided there is no output redirection using >)
            if(input_re) {
                int file_desc1 = open(input_file, O_RDWR); 
                dup2(file_desc1, 0);
                close(file_desc1);
            } 
            if(output_re) {
                int file_desc2 = open(output_file, O_WRONLY | O_APPEND); 
                dup2(file_desc2, 1);
                close(file_desc2);
            }
            else {
                dup2(filedes2[1], STDOUT_FILENO);
            }
        }
        else if (islast){   //if last commmand in the sequence then we only add pipe to its read end only (provided there is no input redirection using <)
            if (input_re) {
                int file_desc1 = open(input_file, O_RDWR); 
                dup2(file_desc1, 0);
                close(file_desc1);
            } else {
                if (pipe_number % 2 != 0){ // for odd number of commands
                    dup2(filedes2[0],STDIN_FILENO);
                }else{ // for even number of commands
                    dup2(filedes[0],STDIN_FILENO);
                }
            }
            if (output_re) {
                int file_desc2 = open(output_file, O_WRONLY | O_APPEND); 
                dup2(file_desc2, 1);
                close(file_desc2);
            }
        } else {    //depending which numbered command it is we decide whether pipe1 will be used as read and pipe2 as write or pipe2 as read and pipe1 as write given no i/o redirection using >,<
            if (pipe_number % 2 != 0){
                if (input_re) {
                    int file_desc1 = open(input_file, O_RDWR); 
                    dup2(file_desc1, 0);
                    close(file_desc1);
                } else {
                    dup2(filedes2[0],STDIN_FILENO);
                }
                if (output_re) {
                    int file_desc2 = open(output_file, O_WRONLY | O_APPEND); 
                    dup2(file_desc2, 1);
                    close(file_desc2);
                } else {
                    dup2(filedes[1],STDOUT_FILENO);
                }
            }else{ // for even i
                if (input_re) {
                    int file_desc1 = open(input_file, O_RDWR); 
                    dup2(file_desc1, 0);
                    close(file_desc1);
                } else {
                    dup2(filedes[0],STDIN_FILENO);
                }
                if (output_re) {
                    int file_desc2 = open(output_file, O_WRONLY | O_APPEND); 
                    dup2(file_desc2, 1);
                    close(file_desc2);
                } else {
                    dup2(filedes2[1],STDOUT_FILENO);
                }
            }
        }
        execvp(args[0],args);
    } else {
        //closing the pipes' ends in the parents
        if (pipe_number == 0){
            close(filedes2[1]);
        }
        else if (islast){
            if (pipe_number % 2 != 0){
                close(filedes2[0]);
            }else{
                close(filedes[0]);
            }
        }else{
            if (pipe_number % 2 != 0){
                close(filedes2[0]);
                close(filedes[1]);
            }else{
                close(filedes[0]);
                close(filedes2[1]);
            }
        }
        int status;
        int corpse = wait(&status);
        if (corpse != -1 && WIFEXITED(status))
        {
            int estat = WEXITSTATUS(status);
            char *err = (estat == 0) ? "success" : "failure";
            if (estat!=0) {printf("Error in executing the command "); PrintArguments(arguments_head); printf("\n");}
            // printf(" PID %d exited with status %d (%s)\n", child, estat, err);
        }
        else {
            printf("Didn't exit was signalled ");
            PrintArguments(arguments_head);
            printf("\n");
        }
        // printf("Number of arguments: %d Starting execution for ",num_args);
        // for (int i = 0;i<num_args+1;i++) {
        //     printf("%s ",args[i]);
        // }
        // printf("Execution completed\n");
    }
}

//for execution of each atomic command in redirection using pipes
void HandleMultiple(struct simple_command* chain_head,int max_size_command) {
    struct simple_command* chain_curr = chain_head;
    int pipe_number = 0;
    while(chain_curr!=NULL) {
        // printf("Command binary: %s",chain_curr->binaryFile);
        // PrintArguments(chain_curr->arguments);
        int islast = 0;
        if (chain_curr->next_simple==NULL) islast = 1;
        SimpleExecution(chain_curr->arguments,max_size_command,chain_curr->num_args,pipe_number,islast);
        chain_curr = chain_curr->next_simple;
        pipe_number = pipe_number+1;
    }
}

int Exec(struct command execute,int max_size_command,int background) {
    //make a thread and execute and calculate the time
    // printf("%d ",execute.command_index);
    // PrintArguments(execute.arguments);
    // printf("\n");

    signal(SIGCHLD, handler);

    int num_pipes = 0;
    struct simple_command *commands_chain_head = execute.basic;
    struct simple_command *commands_chain_curr = commands_chain_head;
    while (commands_chain_curr != NULL) {
        // printf("\nAnother pipe added: %d %s",num_pipes,commands_chain_curr->binaryFile);
        num_pipes = num_pipes + 1;
        commands_chain_curr = commands_chain_curr->next_simple;
    }

    int n_args = execute.num_args+2;
    char** args = malloc(sizeof(n_args*sizeof(char*)));
    int input_re = 0;
    int output_re = 0;
    char* output_file = (char *)malloc(max_size_command+1);
    char* input_file = (char* )malloc(max_size_command+1);
    if (num_pipes == 1 || num_pipes == 0) {
        for (int j = 0;j<n_args;j++){
            args[j] = (char *)malloc(max_size_command+1);
        }
        struct arguments* curr = execute.arguments;
        int i = 0, k = 0;
        for (i=0;i<n_args-1;i++){
            if ( (strcmp(curr->argument,"<") == 0 ) || (strcmp(curr->argument,">") == 0 ) ) {
                    if (strcmp(curr->argument,">") == 0) {
                        output_file = curr->next->argument; 
                        output_re = 1;
                        if (curr->next->next == NULL) break;
                        curr = curr->next->next;
                    } else {
                        input_file = curr->next->argument;
                        input_re = 1;
                        if (curr->next->next == NULL) break;
                        curr = curr->next->next;
                    }
                continue;
            }
            args[k] = curr->argument;
            curr = curr->next;
            k++;
        }
        // printf("%s ",args[k]);
        // PrintCommands(chain_commands);
        args[k] = NULL;
        // printf("Total args w/o redirection: %d Number of pipes %d",k,num_pipes);

    }

    int p[2];
    if (pipe(p) < 0) {
        printf("Pipe failed for inter-process communication");
        return -1;}

    int rc = fork();
    if (rc < 0){
        printf("Fork failed");
        return -1;
    }
    if (rc==0){ 

        if(input_re) {
            int file_desc1 = open(input_file, O_RDWR);
            if (file_desc1 < 0) {
                printf("Error in opening the file %s\n",input_file);
                return -1;
            } 
            dup2(file_desc1, 0);
            close(file_desc1);
        }
        if(output_re) {
            int file_desc2 = open(output_file, O_WRONLY | O_APPEND);
            if (file_desc2 < 0) {
                printf("Error in opening the file %s\n",output_file);
                return -1;
            } 
            dup2(file_desc2, 1);
            close(file_desc2);
        } 

        close(p[0]);
        long time_start;
        time_start = time(0);
        if (write(p[1],&time_start,sizeof(time_start)) == -1) perror("write () error in child");


        if (num_pipes == 1) {
            execvp(execute.binaryFile,args);
        } else {
            // printf("\nMultiple pipes %d %s\n",num_pipes,commands_chain_head->binaryFile);
            // Exec_simple_commands(commands_chain_head,max_size_command,num_pipes);
            // printf("\nExecution started\n");
            struct simple_command* chain_head = commands_chain_head;
            HandleMultiple(chain_head,max_size_command);
        }
        
        // if (errno == ENOENT)
        //     _exit(-1);
        // _exit(-2);
        exit(errno);
    } else {
        if (background) return rc;
        int status;
        int corpse = wait(&status);
        long end_time,start_time;
        end_time = time(0);
        close(p[1]);
        if (read(p[0],&start_time,sizeof(start_time))==-1) perror("read() error in parent");
        printf("---- ");
        // for (int j = 0;j<k;j++) {
        //     printf("%s ",args[j]);
        // }
        PrintArguments(execute.arguments);
        printf(" executed with");
        if (corpse != -1 && WIFEXITED(status))
        {
            int estat = WEXITSTATUS(status);
            char *err = (estat == 0) ? "success" : "failure";
            printf(" PID %d exited with status %d", rc, estat);
            printf(" in %ld ms----\n",end_time-start_time);
        }
        else
            printf(" PID %d and didn't exit; it was signalled", corpse);
        return rc;
        // printf("\nParent %d, %d\n",rc, getpid());
    }
}


void command_prompt(struct commands** head_c,char* prog_name){

    int i = 1;
    char * filename = NULL;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    int comm = 0;
    FILE * fp;
    int total_commands = 0;
    int max_size_command = 0;

    int nn = 0;

    while(nn == 0){
        printf("%s  ",command_shell);
        int argument_list = 0;
        int k = 0;
        int num_arg = 0;
        read = getline(&line,&len,stdin);
        // printf("%s %ld\n",line,len);

        if (line[0] == '\n') continue;

        int count = 0; 
  
        // Traverse the given string. If current character 
        // is not space, then place it at index 'count++' 
        for (int i = 0; line[i]; i++) 
            if (line[i] == '\t') 
                line[count++] = ' '; // here count is 
                                    // incremented 
            else line[count++] = line[i];
        line[count] = '\0'; 

        //extract command

        struct arguments* head = NULL;
        // PrintArguments(head);
        line = strtok(line,"\n");
        char* executable = NULL;
        int background_process = 0;

        int valid = 0;
        for (int i = 0;line[i];i++) {
            if (line[i] != ' ' && line[i] != '\t') {
                valid = 1;
                // printf("Valid string");
            }
        } 
        if (valid == 0) {continue;}
        if (strcmp(line," ") == 0) continue;

        executable = strtok(line," ");
        char* arg = executable;
        addArgument(&head,executable);


        int can_be_back = 0;
        char* backup;
        while(executable!=NULL) {
            // printf("Argument: %s\n",executable);
            if (strlen(executable) > max_size_command) max_size_command = strlen(executable);
            executable = strtok(NULL," ");
            if (executable!=NULL){
                if (can_be_back == 1){
                    addArgument(&head,backup);
                    num_arg = num_arg+1;
                }
                if (strcmp(executable,"&") == 0) {
                    can_be_back = 1;
                    backup = executable;
                } else { 
                    can_be_back = 0;
                    addArgument(&head,executable);
                    num_arg = num_arg+1; 
                }
            } else {
                if (can_be_back == 1) background_process = 1;
            }
        }
        // printf("\nThe arguments linked list is: ");
        // PrintArguments(head);
        struct command exec_command;
        exec_command.num_args = num_arg;
        exec_command.arguments = head;
        exec_command.binaryFile = arg;
        exec_command.command_index = total_commands + 1;
        exec_command.background = 0;
        exec_command.current = 1;
        // printf("\nInfo: %d %s %d %d",exec_command.command_index,exec_command.binaryFile,exec_command.num_args,num_arg);

        struct arguments* curr_arg = head;
        struct arguments* chain_args = NULL;
        struct simple_command* chain_commands = NULL;
        char* starting = curr_arg->argument;
        int start = 0;
        int end = 0;
        int illegal = 0;
        while(curr_arg!=NULL) {
            if (strcmp(curr_arg->argument,"|") == 0) {
                struct simple_command command_in_chain;
                command_in_chain.binaryFile = starting;
                command_in_chain.arguments = chain_args;
                command_in_chain.num_args = end-start;
                // printf("\nNumber of arguments: %d\n",end-start);
                AddSimpleCommand(&chain_commands,command_in_chain);
                chain_args = NULL;
                if (curr_arg->next == NULL) {
                    illegal = 1;
                    break;
                }
                end = end + 1;
                start = end;
                starting = curr_arg->next->argument;
                // chain_args = NULL;
                curr_arg = curr_arg->next;
            } else {
                addArgument(&chain_args,curr_arg->argument);
                curr_arg = curr_arg->next;
                end=end+1;
            }
        }
        if (illegal) {
            printf("Illegal syntax");
            continue;
        }
        struct simple_command command_in_chain;
        command_in_chain.binaryFile = starting;
        command_in_chain.arguments = chain_args;
        command_in_chain.num_args = end - start;
        AddSimpleCommand(&chain_commands,command_in_chain);
        // printf("Basic commands in this:\n");
        // PrintSimpleCommand(chain_commands);
        // printf("Basic commands in this:\n");
        exec_command.basic = chain_commands;


        int pid;
        int same = 1;
        //and call exec based on type
        struct arguments* temp = exec_command.arguments;
        int kkk = (int) strlen(temp->argument);
        if (strcmp(temp->argument,"pid") == 0 && temp->next==NULL){
            //print current process pid
            printf("command name %s process id: %d",prog_name,getpid());
            printf("\n");
            continue;
        } else if (strcmp(temp->argument,"pid") == 0 && strcmp(temp->next->argument,"all") == 0 && temp->next->next == NULL){
            // printf("Print all the spawned processes");
            PrintCommands(*head_c);
            continue;
        } else if (strcmp(temp->argument,"pid") == 0 && strcmp(temp->next->argument,"current") == 0 && temp->next->next == NULL){
            // printf("Print only the current commands");
            printCurrentProc(curr_proc_h);
            continue;
        } else if (strcmp(temp->argument,"cd") == 0 && temp->next!=NULL && temp->next->next == NULL) {
            char *directory = temp->next->argument;
            int ret;
            ret = chdir (directory);
            if (ret == -1) {
                printf("Error in cd\n");
                continue;}
            change_dir();
            continue;
        }
        else if (kkk > 4 && temp->next == NULL){
            char* command_ex;
            command_ex = "HIST";
            for (int j = 0;j<4;j++){
                char* x = temp->argument;
                if (x[j] != command_ex[j]) {
                    same = 0;
                    break;
                }
            }
            // printf("len of %d\n",kkk-4);
            if (same == 1){
                char* number = (char*) malloc(sizeof(char)*(strlen(temp->argument)-4));
                for (int jj = 0;jj<kkk-4;jj++) 
                {
                    char* y = temp->argument;
                    // printf("%c",y[jj+4]);
                    number[jj] = y[jj+4];
                }
                // printf("\n%d\n",atoi(number));
                // printf("\n%d",total_commands);
                PrintHis(*head_c,total_commands,atoi(number));
                continue;
            }
        }
        same = 1;
        if (kkk > 5 && temp->next == NULL) {
            // printf("Entered");
            char* command_ex = NULL;
            command_ex = "!HIST";
            for (int j = 0;j<5;j++){
                char* x = temp->argument;
                if (x[j] != command_ex[j]) {
                    same = 0;
                    break;
                }
            }
            // printf("len of %d\n",kkk-4);
            if (same == 1){
                char* number = (char*) malloc(sizeof(char)*(strlen(temp->argument)-5));
                for (int jj = 0;jj<kkk-5;jj++) 
                {
                    char* y = temp->argument;
                    // printf("%c",y[jj+4]);
                    number[jj] = y[jj+5];
                }
                // printf("\n%d\n",atoi(number));
                // printf("\n%d",total_commands);
                // PrintHis(*head_c,total_commands,atoi(number));
                exec_command = Search(*head_c,1,atoi(number),"\0",-1);
                // printf("%s",exec_command.basic->binaryFile);
                // exec_command.basic = NULL;
                background_process = exec_command.background;
                // printf("%d",exec_history.command_index);
                // printf("\nInfo: %d %s %d %d %d\n",exec_history.command_index,exec_history.binaryFile,exec_history.num_args,exec_history.pid,exec_history.background);
                // PrintArguments(exec_history.arguments);

                // continue;
            }
        }
        total_commands = total_commands + 1;
        exec_command.command_index = total_commands;
        if(background_process == 0) {
            pid = Exec(exec_command,max_size_command,0);
            exec_command.current = 0;    
        }
        else {
            pid = Exec(exec_command,max_size_command,1);
            exec_command.background = 1;
            addCurrentProc(&curr_proc_h,pid,exec_command.binaryFile);
        }
        // printf("\nCHILD PROCESS ID %d\n",pid);

        exec_command.pid = pid;

        addCommand(head_c,exec_command);
        // printf("\nFINAL HEAD IS NOW :");
        // printf("\nCurrently running processes");
        // printCurrentProc(curr_proc_h);
        // PrintCommands(*head_c);
        line = NULL;
        num_arg = 0;
        // printf("\n");

    }
}

char* extract_display_info(int mode){
    char* arguments[2];
    char *result = (char*) malloc(sizeof(char));
    if (mode == 1) arguments[0] = "hostname";
    else if (mode == 2) arguments[0] = "whoami";
    else arguments[0] = "pwd";
    arguments[1] = NULL;

    int p1[2];
    if(pipe(p1) == -1)
    {
        fprintf(stderr, "Error creating pipe\n");
    }

    pid_t child_id;
    child_id = fork();
    if(child_id == -1)
    {
        fprintf(stderr, "Fork error\n");
    }
    if(child_id == 0) // child process
    {
        close(p1[0]); // child doesn't read
        dup2(p1[1], 1); // redirect stdout

        execvp(arguments[0], arguments);

        fprintf(stderr, "Exec failed\n");
    }
    else
    {
        wait(NULL);
        close(p1[1]); // parent doesn't write
        char* reading_buf = (char*) malloc(sizeof(char));
        int j = 0;
        while(read(p1[0], reading_buf, 1) > 0)
        {   
            char* updated_result = (char*) malloc((j+1)*sizeof(char));
            for (int k = 0; k < j;k++) {
                updated_result[k] = result[k];
                // printf("%s %d \t",updated_result[k],k);
            }
            if (strcmp(reading_buf,"\n") != 0) updated_result[j] = reading_buf[0];
            result = NULL;
            result = updated_result;
            // printf("%d %s %s\n",j,result,reading_buf);
            j = j + 1;
            // write(1, reading_buf, 1); // 1 -> stdout
        }
        // printf("%s",result);
        close(p1[0]);
        return result;
    }
}


void change_dir() {
    // hostname = extract_display_info(1);
    // printf("Extracted hostname successfully");
    // username = extract_display_info(2);
    pwd = extract_display_info(3);
    int total_len = strlen(hostname) + strlen(username) + strlen(pwd);
    command_shell = (char*) malloc((total_len+1+4)*sizeof(char));
    if (command_shell) {
        memcpy(command_shell,"<",1);
        memcpy(command_shell + 1 ,username, strlen(username));
        memcpy(command_shell + 1 + strlen(username),"@",1);
        memcpy(command_shell + 2 +  strlen(username), hostname, strlen(hostname));
        memcpy(command_shell + 2 + strlen(hostname) + strlen(username), ":",1);
        memcpy(command_shell + 3 + strlen(hostname) + strlen(username), pwd, strlen(pwd));
        memcpy(command_shell + 3 + strlen(hostname) + strlen(username) + strlen(pwd), ">",2);
    }
}


void sigproc()
{ 
    signal(SIGINT, sigproc); /*  */
}