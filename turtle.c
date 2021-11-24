#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>



#define MAX_INPUT_SIZE 1024
#define MAX_TOKEN_SIZE 64
#define MAX_NUM_TOKENS 64

////// Builtin functions ///////////////////////////////////////////////
int turtle_cd(char **tokens);
int turtle_help(char **tokens);
int turtle_exit(char **tokens);

char *builtin_commands[] = {"cd","help","exit"};
int (*builtin_funcs[]) (char **) = {
    &turtle_cd,
    &turtle_help,
    &turtle_exit
};
int builtin_num()
{
    return sizeof(builtin_commands)/(sizeof(char *));
}



int turtle_exit(char **args)
{
  return 0;
}

int turtle_help(char **args)
{
  int i;
  printf("Type program names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i = 0; i < builtin_num(); i++) {
    printf("  %s\n", builtin_commands[i]);
  }

  printf("Use the man command for information on other programs.\n");
  return 1;
}


int turtle_cd(char **args)
{
  if (args[1] == NULL) {
    fprintf(stderr, "turtle: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("turtle");
    }
  }
  return 1;
}

//////////////////////////////////////////////////////////////////////////

////// Signal Handlers ///////////////////////////////////////////////////

void signal_handler(int signal_number)
{
  if(signal_number==SIGINT)
  {
    printf("\n");
    printf("turtle > ");
    fflush(stdout);
  }
}

void signal_handler2(int signal_number)
{
  if(signal_number==SIGINT)
  {
    printf("\n");
    fflush(stdout);
  }
}

/////////////////////////////////////////////////////////////////////////////

////////////////////////// Piping ///////////////////////////////////////////
int exec_pipe(char** parsedpipe, char** parsed)
{
    int fd[2];
    if (pipe(fd)) {
       perror("Error creating pipe\n");
       exit(EXIT_FAILURE);
    }
        
    if (fork() == 0) {
        signal(SIGINT,SIG_DFL);
        close(fd[0]);  
        signal(SIGINT,SIG_DFL);
        dup2(fd[1], STDOUT_FILENO);
        signal(SIGINT,SIG_DFL);
        close(fd[1]);
        if(execvp(parsed[0], parsed)<0)    
            perror("Execution Failure");
        exit(EXIT_SUCCESS);
    }

    signal(SIGINT,signal_handler2);

    if (fork() == 0) {
        signal(SIGINT,SIG_DFL);
        close(fd[1]); 
        signal(SIGINT,SIG_DFL);
        dup2(fd[0], STDIN_FILENO);
        signal(SIGINT,SIG_DFL);
        close(fd[0]);
        if(execvp(parsedpipe[0], parsedpipe)==-1)
            perror("Execution Failure");
        exit(EXIT_SUCCESS);
    }
    signal(SIGINT,signal_handler2);
    close(fd[0]);
    close(fd[1]);

    wait(NULL);
    wait(NULL);
    signal(SIGINT,signal_handler);

    //free(parsed);
    //free(parsedpipe);
    return 1;

}

int run_pipes(char** cmd) {
    //for(int i=0; i<num; i++);
    int i = 0;
    int pipes = 0;
    for (i = 0; cmd[i]!=NULL; i++) {
        if (!strcmp(cmd[i], "|"))
        { 
            pipes = 1 ;break;
        }
    }
    if (!pipes) return 0;
    char **in = (char **)malloc(i * sizeof(char *));
    char **out= (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
    int j=0;
    for (; j < i; j++)
        {
            //in[j] = cmd[j];
            in[j] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
            strcpy(in[j],cmd[j]);
        }
        in[i] = NULL;
    for (j = i + 1; cmd[j]!=NULL; j++)
        {
            out[j - i - 1] = cmd[j];
        }
        out[j-i-1] = NULL;
    return exec_pipe(out,in);
}
//////////////////////////////////////////////////////////////////////////////

/////////////////// Redirection //////////////////////////////////////////////

/*
int redirection_shell_execute(char **tokens)
{

    if(detExecPipes(tokens)) return 1;
    else
    {
        if(tokens[0]==NULL) return 1;
        //check if builtin
        int num_builtins = builtin_num();
        for(int i=0;i<num_builtins;i++)
        {
            if(strcmp(tokens[0],builtin_commands[i])==0)
            {
                return (*builtin_funcs[i])(tokens);
            }
        }
        //else system call
        return shell_run(tokens);
    }
}
*/
int exec_redirect(char** cmd, int pos) {
    pid_t pid,wpid;
    int status;
    char **in = (char **)malloc(pos * sizeof(char *));
    for(int i = 0; i<pos; i++) {
        in[i] = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));
        strcpy(in[i],cmd[i]);
    }
    in[pos] = NULL;
    char* filename = cmd[pos+1];
    pid = fork();
    if (pid == 0) {
        signal(SIGINT,SIG_DFL);
        int fd1 = open(filename, O_CREAT |  O_RDWR, S_IRWXU);
        if(fd1 < 0)
        {
            fprintf(stderr, "turtle: Open failed \n");
            //printf("Open failed \n");
            exit(1);
        }
        dup2(fd1, 1);
        if (execvp(in[0], in)==-1) {
                perror("turtle");
                exit(EXIT_FAILURE);
            }
        close(fd1);
    } 
    else 
    { //Parent Process
        do 
        {
          signal(SIGINT,signal_handler2);
          wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    signal(SIGINT,signal_handler);
    return 1;
    //free(in);
}

int check_redirection(char** cmd) {
    for(int i = 0; cmd[i] != NULL; i++)
        if (!strcmp(cmd[i], ">"))
            return i;
    return 0;
}



//////////////////////////////////////////////////////////////////////////////

/////// Command Reading //////////////////////////////////////////////////////
char *read_line()
{
    char *line = NULL;
    size_t bufsize = 0;
    int status = getline(&line,&bufsize,stdin);
    if(status == -1)
    {
        if(feof(stdin))
        {
            exit(EXIT_SUCCESS);
        } 
        else {
            perror("readline");
            exit(EXIT_FAILURE);
            }
    }
    return line;
}


char **tokenize(char *line)
{
    char **tokens = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
    char *token = (char *)malloc(MAX_TOKEN_SIZE * sizeof(char));

    if(!tokens || !token)
    {
    	fprintf(stderr,"turtle: allocation error\n");
    	exit(EXIT_FAILURE);
    }

    int i, tokenIndex = 0, tokenNo = 0;

    for(i =0; i < strlen(line); i++){

        char readChar = line[i];

        if (readChar == ' ' || readChar == '\n' || readChar == '\t') {
            token[tokenIndex] = '\0';
            if (tokenIndex != 0){
                tokens[tokenNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
                if(!tokens[tokenNo])
                {
                    fprintf(stderr,"turtle: allocation error\n");
                    exit(EXIT_FAILURE);
                }
                strcpy(tokens[tokenNo++], token);
                tokenIndex = 0; 
            }
        } 
        else {
            token[tokenIndex++] = readChar;
        }
    }
 
    free(token);
    tokens[tokenNo] = NULL ;
    return tokens;
}
////////////////////////////////////////////////////////////////////////////



///// Shell Executing //////////////////////////////////////////////////////

int shell_run(char **tokens)
{
    pid_t pid,wpid;
    int status;

    pid=fork();
    if(pid==0) //child
    {
        signal(SIGINT,SIG_DFL);
        if(execvp(tokens[0],tokens) == -1)
        {
            perror("turtle");
        }
        exit(EXIT_FAILURE);
    }
    else if(pid < 0)
    {
        //forking error
        perror("turtle");
    }
    else{
    // Parent process
    do {
      signal(SIGINT,signal_handler2);
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    signal(SIGINT,signal_handler);
  }
  return 1;
}

int shell_execute(char **tokens)
{
    int pos = check_redirection(tokens);
    if(pos)
    {
         return exec_redirect(tokens,pos);
         //return 1;
    }
    else if(run_pipes(tokens)) return 1;
    else
    {
        if(tokens[0]==NULL) return 1;
        //check if builtin
        int num_builtins = builtin_num();
        for(int i=0;i<num_builtins;i++)
        {
            if(strcmp(tokens[0],builtin_commands[i])==0)
            {
                return (*builtin_funcs[i])(tokens);
            }
        }
        //else system call
        return shell_run(tokens);
    }
}

///////////////////////////////////////////////////////////////////////////////////




////////// Driver Codes ///////////////////////////////////////////////////////////
void shell_loop()
{
    char *line;
    char **cmd;
    char **tokens;
    int status = 1;
    while(status)
    {
        cmd = (char **)malloc(MAX_NUM_TOKENS * sizeof(char *));
        printf("turtle > ");
        //get line
        line = read_line();
        tokens = tokenize(line);
        int i;
        int cmdNo = 0;
        for(i=0;tokens[i]!=NULL;i++)
        {
            //printf("i = %d\n",i);
            if(!strcmp(tokens[i],";;"))
            {
                cmd[cmdNo] = NULL;
                status = shell_execute(cmd);
                cmdNo = 0;
                //free(cmd);
                if(!status)
                {
                    free(tokens);
                    free(line);    
                    return;
                }
            }
            else
            {
                cmd[cmdNo] = (char*)malloc(MAX_TOKEN_SIZE*sizeof(char));
                strcpy(cmd[cmdNo++],tokens[i]);
            }  
        }
        if(cmdNo!=0)
        {
            cmd[cmdNo] = NULL;
            status = shell_execute(cmd);
            //free(cmd); 
        }
        free(cmd);
        free(tokens);
        free(line);
    }
}

int main()
{
    //config files if any
    signal(SIGINT,signal_handler);

    //Shell loop
    shell_loop();

    //Perform clearing or shutdown

    return EXIT_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////////

