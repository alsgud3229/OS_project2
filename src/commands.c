#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <sys/wait.h>

#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <pthread.h>

#include "commands.h"
#include "built_in.h"

#define SERVER_PATH "tpf_unix_sock.server"
#define CLIENT_PATH "tpf_unix_sock.client"

static struct built_in_command built_in_commands[] = {
  { "cd", do_cd, validate_cd_argv },
  { "pwd", do_pwd, validate_pwd_argv },
  { "fg", do_fg, validate_fg_argv }
};

static int is_built_in_command(const char* command_name)
{
  static const int n_built_in_commands = sizeof(built_in_commands) / sizeof(built_in_commands[0]);

  for (int i = 0; i < n_built_in_commands; ++i) {
    if (strcmp(command_name, built_in_commands[i].command_name) == 0) {
      return i;
    }
  }
  return -1; // Not found
}

//    /usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
static void resol_path(struct single_command* com){
  execv(com->argv[0], com->argv);
  char PATH[512]; 
  char* nextptr = NULL;
  char* tok;
  sprintf(PATH, "%s", getenv("PATH"));
  tok = strtok_r(PATH, ":", &nextptr);
  while(tok){
    char path[512];
    strcpy(path, tok);
    strcat(path, "/");
    strcat(path, com->argv[0]);
    execv(path, com->argv);
    tok = strtok_r(NULL, ":", &nextptr);
  }
}
//
int evaluate_command(int n_commands, struct single_command (*commands)[512]);

//
void* thread_func(void * parameter){
  char** com = ((char **)parameter);
  int ti = 0;

  for(int i=0; com[i]!=NULL; i++) {
    ti++;
  }
  struct single_command this_com[512] = {ti, com};

  // Create a Client socket 
  int client_sock, rc, len;
  struct sockaddr_un server_sockaddr;
  struct sockaddr_un client_sockaddr;
  memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));
  memset(&client_sockaddr, 0, sizeof(struct sockaddr_un));
  // Create a UNIX domain stream socket
  client_sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if(client_sock == -1)
    exit(1);
  // Set up the UNIX sockaddr structure by using AF_UNIX for the family and giving it a filepath to bind to.
  client_sockaddr.sun_family = AF_UNIX;
  strcpy(client_sockaddr.sun_path, CLIENT_PATH);
  len = sizeof(client_sockaddr);
  // Unlink the file so the bind will suceed, then bind to that file.
  unlink(CLIENT_PATH);
  rc = bind(client_sock, (struct sockaddr *)&client_sockaddr, len);
  if(rc == -1){
    close(client_sock);
    exit(1);
  }
  // Set up the UNIX sockaddr structure for the server socket and connect to it.
  server_sockaddr.sun_family = AF_UNIX;
  strcpy(server_sockaddr.sun_path, SERVER_PATH);
  rc = connect(client_sock, (struct sockaddr *)&server_sockaddr, len);
  if(rc == -1){
    close(client_sock);
    exit(1);
  }

  int outstream = dup(1);
  dup2(client_sock, 1);
  evaluate_command(1, &this_com);
  close(client_sock);
  fflush(stdout);
  dup2(outstream, 1);
  close(1);

  pthread_exit(NULL);
}
/*
 * Description: Currently this function only handles single built_in commands. You should modify this structure to launch process and offer pipeline functionality.
 */
int evaluate_command(int n_commands, struct single_command (*commands)[512])
{
  struct single_command* com = (*commands);
    assert(com->argc != 0);
  if(n_commands == 1) {
    // struct single_command* com = (*commands);
    // assert(com->argc != 0);
    int built_in_pos = is_built_in_command(com->argv[0]);

    if (built_in_pos != -1) {
      if (built_in_commands[built_in_pos].command_validate(com->argc, com->argv)) {
        if (built_in_commands[built_in_pos].command_do(com->argc, com->argv) != 0) {
          fprintf(stderr, "%s: Error occurs\n", com->argv[0]);
        }
      } 
      else {
        fprintf(stderr, "%s: Invalid arguments\n", com->argv[0]);
        return -1;
      }
    } 
    else if (strcmp(com->argv[0], "") == 0) {
      return 0;
    } 
    else if (strcmp(com->argv[0], "exit") == 0) {
      return 1;
    } 
    else {     
      pid_t pid;
      pid_t pid_child;
      int child_status;

      pid = fork();

      if(pid == -1){
         printf( "Fork falied\n");
         return -1;
      }
      else if(pid == 0) {
        //printf("나는 차일드\n");
        resol_path(com);
        fprintf(stderr, "%s: command not found\n", com->argv[0]);
        exit(1);
      }
      else{
        //printf("나는 부모 웨잇전\n");
        pid_child = wait(&child_status);
        //printf( "나는 부모, wait종료, 종료된 자식 프로세스 ID는 %d\n", pid_child);
        return 0;
      }
    }
  } 
  if(n_commands >= 2){
    // struct single_command* com = (*commands);
    // assert(com->argc != 0);
    
    int server_sock, client_sock, len, rc;
    struct sockaddr_un server_sockaddr;
    struct sockaddr_un client_sockaddr;
    int backlog = 10;
    memset(&server_sockaddr, 0, sizeof(struct sockaddr_un));
    memset(&client_sockaddr, 0, sizeof(struct sockaddr_un));

    // Create a UNIX domain stream socket
    server_sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if(server_sock == -1)
     exit(1);
    // Set up the UNIX sockaddr structure by using AF_UNIX for the family and giving it a filepath to bind to.
    server_sockaddr.sun_family = AF_UNIX;
    strcpy(server_sockaddr.sun_path, SERVER_PATH);
    len = sizeof(server_sockaddr);
    // Unlink the file so the bind will suceed, then bind to that file.
    unlink(SERVER_PATH);
    rc = bind(server_sock, (struct sockaddr *)&server_sockaddr, len);
    if(rc == -1){
      close(server_sock);
      exit(1);
    }
    // Listen for any client sockets
    rc = listen(server_sock, backlog);
    if(rc == -1){
      close(server_sock);
      exit(1);
    }
    // Server is now blocked, so we should create thread to handle it.
    pthread_t threads[3];
    int status;

    rc = pthread_create(&threads[0], NULL, thread_func, (void*)com->argv);
    if(rc == -1)
      exit(1);
    // then comeback to server and accept it
    client_sock = accept(server_sock, (struct sockaddr *)&client_sockaddr, &len);
    if(client_sock == -1){
      close(server_sock);
      close(client_sock);
      exit(1);
    }
    // Join thread
    pthread_join(threads[0], (void**)&status);
    // Do fork for second command handling
    pid_t pid;
    pid_t pid_child;
    int child_status;
    int instream = dup(0);
    pid = fork();

    if(pid == -1){
      printf( "Fork failed\n");
      return -1;
    }
    else if(pid == 0) {
      //printf("나는 차일드\n");
      dup2(client_sock, 0);
      resol_path(com+1);
      fprintf(stderr, "%s: command not found\n", (com+1)->argv[0]);
      close(client_sock);
      dup2(instream, 0);
      close(0);
      exit(0);
    }
    else{
      //printf("나는 부모 웨잇전\n");
      close(client_sock);
      pid_child = wait(&child_status);
     // printf( "나는 부모, wait종료, 종료된 자식 프로세스 ID는 %d\n", pid_child);
      return 0;
    }
    close(server_sock);
    close(client_sock);
    return 0;
  }
  return 0;
}

void free_commands(int n_commands, struct single_command (*commands)[512])
{
  for (int i = 0; i < n_commands; ++i) {
    struct single_command *com = (*commands) + i;
    int argc = com->argc;
    char** argv = com->argv;

    for (int j = 0; j < argc; ++j) {
      free(argv[j]);
    }

    free(argv);
  }

  memset((*commands), 0, sizeof(struct single_command) * n_commands);
}
