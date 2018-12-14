#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#include "commands.h"
#include "built_in.h"
#include "utils.h"
#include "signal_handlers.h"

int main()
{
  putenv("PATH=/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin");
  //printf("%s\n", getenv("PATH"));
  char buf[8096];

  signal(SIGINT,catch_sigint);
  signal(SIGTSTP,catch_sigtstp);

  while (1) {
    fflush(stdout);

    fgets(buf, 8096, stdin);

    struct single_command commands[512];
    int n_commands = 0; //pipe가 쓰이지 않는한 항상 1일것.
    mysh_parse_command(buf, &n_commands, &commands);
    //printf("\n");
    int ret = evaluate_command(n_commands, &commands);

    free_commands(n_commands, &commands);
    n_commands = 0;

    if (ret == 1) {
      break;
    }
  }

  return 0;
}
