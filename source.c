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

struct arguments
{
    char* argument;
    struct arguments* next;
};

struct command
{
    /* data */
    int pid;
    int background;
    int current;
    int num_args;
    int command_index;
    char* binaryFile;
    struct arguments* arguments;
};

struct commands
{
    struct command det_com;
    struct commands *command_next;
};

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
void deleteCurrentProc(struct current_processes** curr_proc_head,int pid);
void printCurrentProc(struct current_processes* curr_proc_h);
void addCurrentProc(struct current_processes** curr_proc_head,int pid,char* binaryFile);
void printCurrentProc(struct current_processes* curr_proc_h);
void change_dir();
struct command Search(struct commands* head,int mode,int num,char* name,int pid);
void PrintArguments(struct arguments* head);
void addCommand(struct commands** head_,struct command add);
void addArgument(struct arguments** head,char* arg);
void PrintHis(struct commands* head,int total_commands,int N);
int Exec(struct command execute,int max_size_command);
int Exec_background(struct command execute,int max_size_command);
void command_prompt(struct commands** head_c,char* prog_name);
char* extract_display_info(int mode);
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
                printf("--- %s executed with PID %d exited with status %d (%s)----\n",comp.binaryFile, p, estat, err);
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

int Exec(struct command execute,int max_size_command) {
    //make a thread and execute and calculate the time
    // printf("%d ",execute.command_index);
    // PrintArguments(execute.arguments);
    // printf("\n");
    int n_args = execute.num_args+2;
    char** args = malloc(sizeof(n_args*sizeof(char*)));
    for (int j = 0;j<n_args;j++){
        args[j] = (char *)malloc(max_size_command+1);
    }
    struct arguments* curr = execute.arguments;
    int i = 0;
    for (i=0;i<n_args-1;i++){
        args[i] = curr->argument;
        curr = curr->next;
    }
    args[i] = NULL;

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
        close(p[0]);
        long time_start;
        time_start = time(0);
        if (write(p[1],&time_start,sizeof(time_start)) == -1) perror("write () error in child");
        execvp(execute.binaryFile,args);
        // if (errno == ENOENT)
        //     _exit(-1);
        // _exit(-2);
        exit(errno);
    } else {
        int status;
        int corpse = wait(&status);
        long end_time,start_time;
        end_time = time(0);
        close(p[1]);
        if (read(p[0],&start_time,sizeof(start_time))==-1) perror("read() error in parent");
        printf("---- ");
        for (int j = 0;j<n_args-1;j++) {
            printf("%s ",args[j]);
        }
        printf("executed with");
        if (corpse != -1 && WIFEXITED(status))
        {
            int estat = WEXITSTATUS(status);
            char *err = (estat == 0) ? "success" : "failure";
            printf(" PID %d exited with status %d (%s)", rc, estat, err);
            printf(" in %ld ms----\n",end_time-start_time);
        }
        else
            printf(" PID %d and didn't exit; it was signalled", corpse);
        return rc;
        // printf("\nParent %d, %d\n",rc, getpid());
    }
}

int Exec_background(struct command execute,int max_size_command) {

    signal(SIGCHLD, handler);

    int n_args = execute.num_args+2;
    char** args = malloc(sizeof(n_args*sizeof(char*)));
    for (int j = 0;j<n_args;j++){
        args[j] = (char *)malloc(max_size_command+1);
    }
    struct arguments* curr = execute.arguments;
    int i = 0;
    for (i=0;i<n_args-1;i++){
        args[i] = curr->argument;
        curr = curr->next;
    }
    args[i] = NULL;

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
        close(p[0]);
        long time_start;
        time_start = time(0);
        if (write(p[1],&time_start,sizeof(time_start)) == -1) perror("write () error in child");
        close(p[1]);
        // int fd = open("fileName", O_RDWR| O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);

        // dup2(fd, 1);
        execvp(execute.binaryFile,args);
        // if (errno == ENOENT)
        //     _exit(-1);
        // _exit(-2);
        exit(errno);
    } else {
        return rc;
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
            pid = Exec(exec_command,max_size_command);
            exec_command.current = 0;    
        }
        else {
            pid = Exec_background(exec_command,max_size_command);
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