#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h> // for waitpid
#include <signal.h>
#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

char *words[MAX_WORDS];
size_t wordsplit(char const *line);
char * expand(char const *word);
int status = 0;
pid_t bgrProcess = -10;

void sigint_handler(int sig){
};

int main(int argc, char *argv[])
{
  FILE *input = stdin;
  char *input_fn = "(stdin)";
  if (argc == 2) {
    input_fn = argv[1];
    input = fopen(input_fn, "re");
    if (!input) err(1, "%s", input_fn);
  } else if (argc > 2) {
    errx(1, "too many arguments");
  }
  
  char *line = NULL;
  size_t n = 0;
  
  struct sigaction SIGINT_action = {0}, SIGINT_default = {0}, SIGINT_Ignore = {0}, 
                   SIGTSTP_action = {0}, SIGTSTP_default = {0};

  SIGINT_action.sa_handler = sigint_handler;
  SIGTSTP_action.sa_handler = SIG_IGN;
  SIGINT_Ignore.sa_handler = SIG_IGN;

  sigfillset(&SIGINT_action.sa_mask);
  sigfillset(&SIGTSTP_action.sa_mask);
  sigfillset(&SIGINT_Ignore.sa_mask);


  SIGINT_action.sa_flags = 0;
  SIGTSTP_action.sa_flags = 0;
  SIGINT_Ignore.sa_flags = 0;

  sigaction(SIGINT, &SIGINT_action, &SIGINT_default);
  sigaction(SIGTSTP, &SIGTSTP_action, &SIGTSTP_default);
  
  int cStatus;
  pid_t retValue;

  for (;;) {
    prompt:;
    /* TODO: Manage background processes */
      retValue = waitpid(0, &cStatus, WUNTRACED | WNOHANG);
      if (retValue > 0){
        if(WIFEXITED(cStatus))
          fprintf(stderr, "Child process %d done. Exit status %d.\n", retValue, WEXITSTATUS(cStatus));
        if(WIFSIGNALED(cStatus))
          fprintf(stderr, "Child process %d done. Signaled %d.\n", retValue, WTERMSIG(cStatus));
        if(WIFSTOPPED(cStatus)){
          if(kill(retValue, SIGCONT) == -0) fprintf(stderr, "Child process %d stopped. Continuing.\n", retValue);
          else{
            perror("kill");
            status = errno;
          }
        }
      }
      if (retValue == -1){
        if(errno != ECHILD) {
          perror("waitpid");
        }
      }
    
    /* TODO: prompt */
    if (input == stdin) {
      fprintf(stderr,"%s", getenv("PS1"));
    }
    ssize_t line_len = getline(&line, &n, input);
    if(errno == EINTR) {
      fprintf(stderr, "\n");
      clearerr(stdin);
      goto prompt;
    }
    sigaction(SIGINT, &SIGINT_Ignore, NULL);
    if (line_len < 0) exit(status);
    size_t nwords = wordsplit(line);
    if (nwords == 0) goto prompt;
    for (size_t i = 0; i < nwords; ++i) {
      char *exp_word = expand(words[i]);
      free(words[i]);
      words[i] = exp_word;
    }
    // built in cd command
    if (strcmp(words[0], "cd") == 0) {
      if (nwords > 2){
        printf("smallsh: cd: too many arguments\n");
        status = 1;
        goto prompt;
      }
      if(nwords == 1){
        if(chdir(getenv("HOME"))!=0) {
          status = 1;
          perror("smallsh: chdir");
          goto prompt;
        }
      }
      else if (chdir(words[1]) != 0){
        status = 1;
        perror("smallsh: chdir");
        goto prompt;
      } 
      status = 0;
      goto prompt;
    }
    

    // built in exit command
    if(strcmp(words[0], "exit") == 0){
      if (nwords > 2) {
        fprintf(stderr,"smallsh: exit: too many arguments\n");
        status = 1;
        goto prompt;
      }
      if (nwords == 1) {
        exit(status);
      }
      if (nwords == 2) {
         long int x;
        char * pEnd;
        x = strtol(words[1], &pEnd, 10);
        if (strcmp(pEnd, "\0") == 0){
          status = x;
          exit(status);
        }
        else{
          status = 1;
          fprintf(stderr,"smallsh: %s: not a number\n", words[1]);
          goto prompt;
        }
      }
    }
    // make a new array to pass to exec. need to do parsing if files and stuff!
    char *arguments[MAX_WORDS] = {NULL};
    char *redirect[MAX_WORDS] = {NULL};
    int l = 0; // redirect index tracker
    int a = 0; // argument index tracker
    for(int i = 0; i < nwords; ++i){
      //check for any operators
      if(strcmp(words[i], "<") == 0){
        redirect[l] = words[i];
        ++l;
        if(i < nwords - 1) {
          redirect[l] = words[i+1];
          ++l;
        }
        ++i;
        continue;
      }
      if(strcmp(words[i], ">") == 0){
        redirect[l] = words[i];
        ++l;
        if(i < nwords - 1){
          redirect[l] = words[i+1];
          ++l;
        }
        ++i;
        continue;
      }
      if(strcmp(words[i], ">>") == 0){
        redirect[l] = words[i];
        ++l;
        if (i < nwords -1) {
          redirect[l] = words[i+1];
          ++l;
        }
        ++i;
        continue;
      }
      if(strcmp(words[i], "&") == 0)continue;
      if(i < nwords) {
        arguments[a] = words[i];
        ++a;
      }
    }

    //non built in commands
    int childStatus;

    pid_t spawnPid = fork();
    bgrProcess = spawnPid;
    switch(spawnPid){
    case -1:
            perror("fork()\n");
            status = 1;
            goto prompt;
    case 0:
              //in the child process.
            sigaction(SIGINT, &SIGINT_default, NULL);
            sigaction(SIGTSTP, &SIGTSTP_default, NULL);
            if(l != 0){
              for(int i = 0; i < l; ++i){
                if(strcmp(redirect[i], "<") == 0){
                  int sourceFD = open(redirect[i+1], O_RDONLY);
                  if(sourceFD == -1){
                    perror("source open()");
                    status = 1;
                    goto prompt;
                  }
                  int result = dup2(sourceFD, 0);
                  if(result == -1){
                    perror("source dup2()");
                    status = 2;
                    goto prompt;
                  }
                }
                if(strcmp(redirect[i], ">") == 0){
                  int targetFD = open(redirect[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0777);
                  if(targetFD == -1){
                    perror("target open()");
                    status = 1;
                    goto prompt;
                  }
                  int result = dup2(targetFD, 1);
                  if (result == -1){
                    perror("target dup2()");
                    status = 2;
                    goto prompt;
                  }
                }
                if(strcmp(redirect[i], ">>") == 0){
                  int targetFD = open(redirect[i+1], O_WRONLY | O_CREAT | O_APPEND, 0777);
                  if(targetFD == -1){
                    perror("target open()");
                    status = 1;
                    goto prompt;
                  }
                  int result = dup2(targetFD, 1);
                  if (result == -1){
                    perror("target dup2()");
                    status = 2;
                    goto prompt;
                  }
                }
              }
            }

              //printf("CHILD(%d) runing %s command\n", getpid(), words[0]);
            if(strcspn(words[0], "/") == 0) {
              execv(words[0], arguments);
              fprintf(stderr, "smallsh: %s: %s\n", words[0], strerror(errno));
              status = 1;
              goto prompt;
            }
            else {
              execvp(words[0], arguments);
              fprintf(stderr,"smallsh: %s: %s\n", words[0], strerror(errno));
              status = 1;
              goto prompt;
            }
    default:
            // In the parent process.
            // wait for child's termination?
            if(strcmp(words[nwords-1], "&") != 0){
              //printf("Waiting!");
              spawnPid = waitpid(spawnPid, &childStatus, WUNTRACED);
              if(spawnPid == -1) {
                perror("waitpid");
                status = 1;
              }
              if (WIFEXITED(childStatus)) status = WEXITSTATUS(childStatus);
              if (WIFSIGNALED(childStatus)) status = 128 + (WTERMSIG(childStatus));
              if (WIFSTOPPED(childStatus)) {
                if(kill(spawnPid, SIGCONT) == 0){
                  fprintf(stderr, "Child process %d stopped. Continuing.\n", spawnPid);
                  bgrProcess = spawnPid;
                }
                else {
                  perror("kill");
                  status = errno;
                }
              }           
            } 
            goto prompt;
    }
    goto prompt;
  }
}

char *words[MAX_WORDS] = {0};

/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 */
size_t wordsplit(char const *line) {
  size_t wlen = 0;
  size_t wind = 0;

  char const *c = line;
  for (;*c && isspace(*c); ++c); /* discard leading space */

  for (; *c;) {
    if (wind == MAX_WORDS) break;
    /* read a word */
    if (*c == '#') break;
    for (;*c && !isspace(*c); ++c) {
      if (*c == '\\') ++c;
      void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
      if (!tmp) err(1, "realloc");
      words[wind] = tmp;
      words[wind][wlen++] = *c; 
      words[wind][wlen] = '\0';
    }
    ++wind;
    wlen = 0;
    for (;*c && isspace(*c); ++c);
  }
  return wind;
}

/* Find next instance of a parameter within a word. Sets
 * start and end pointers to the start and end of the parameter
 * token.
 */
char
param_scan(char const *word, char const **start, char const **end)
{
  static char const *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = 0;
  *end = 0;
  for (char const *s = word; *s && !ret; ++s) {
    s = strchr(s, '$');
    if (!s) break;
    switch (s[1]) {
    case '$':
    case '!':
    case '?':
      ret = s[1];
      *start = s;
      *end = s + 2;
      break;
    case '{':;
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = s[1];
        *start = s;
        *end = e + 1;
      }
      break;
    }
  }
  prev = *end;
  return ret;
}

/* Simple string-builder function. Builds up a base
 * string by appending supplied strings/character ranges
 * to it.
 */
char *
build_str(char const *start, char const *end)
{
  static size_t base_len = 0;
  static char *base = 0;

  if (!start) {
    /* Reset; new base string, return old one */
    char *ret = base;
    base = NULL;
    base_len = 0;
    return ret;
  }
  /* Append [start, end) to base string 
   * If end is NULL, append whole start string to base string.:
   * Returns a newly allocated string that the caller must free.
   */
  size_t n = end ? end - start : strlen(start);
  size_t newsize = sizeof *base *(base_len + n + 1);
  void *tmp = realloc(base, newsize);
  if (!tmp) err(1, "realloc");
  base = tmp;
  memcpy(base + base_len, start, n);
  base_len += n;
  base[base_len] = '\0';

  return base;
}

/* Expands all instances of $! $$ $? and ${param} in a string 
 * Returns a newly allocated string that the caller must free
 */
char *
expand(char const *word)
{
  char const *pos = word;
  char const *start, *end;
  char c = param_scan(pos, &start, &end);
  free(build_str(NULL, NULL));
  build_str(pos, start);
  while (c) {
    if (c == '!') {
      if (bgrProcess == -10) {
        build_str("", NULL);
      }
      else{
        char str[50];
        sprintf(str, "%i", bgrProcess);
        build_str(str, NULL);
      }
    }
    else if (c == '$') {
      char str[50];
      sprintf(str, "%i", getpid());
      build_str(str, NULL);
    }
    else if (c == '?') {
      char str[50];
      sprintf(str, "%i", status);
      build_str(str, NULL);
    }
    else if (c == '{') {
      char env[50] = {};
      char str[1000];
      strncpy(env,start+2, end-start-3);
      if (getenv(env)== NULL) {
        build_str("",NULL);
      } else{
        sprintf(str, "%s", getenv(env));
        build_str(str, NULL);
      }
    }
    pos = end;
    c = param_scan(pos, &start, &end);
    build_str(pos, start);
  }
  return build_str(start, NULL);
}
