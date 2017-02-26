/* 
 * tsh - A tiny shell program with job control
 * Andrew ID: mimic1
 * @auther Mimi Chen
 */
/*
 *As for my shell, I mask every signal when I need to implement
 *add jobs or delete jobs and send signal (kill(pid,sig)) to
 * in case some signal will interrupt this methods and cause some
 * race condition. In order to reduce race condition, I only use
 * waitpid. try to improve the performance I use the sigsuspend
 * to wait for foreground process finished.
 * To fulfill Ctrl_C or Ctrl_Z, I use kill(-pid,sig) to send signal
 * to every foreground process.
 * When it figure out that it was a none-build in command, I need to fork
 * a child process to implement it, meanwhile I had to add this job to the
 * job list, to do that, I had to check the state of the job  whether it has &
 * in the command. If it is a BG, I set the state to BG.
 * When it is a build-in command, such as Jobs or fg, it will be execute
 * immediately,I only have to mask the signal which will interrupt its execution.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include "csapp.h"

/* Misc manifest constants */
#define MAXLINE_TSH    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF         0   /* undefined */
#define FG            1   /* running in foreground */
#define BG            2   /* running in background */
#define ST            3   /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Parsing states */
#define ST_NORMAL   0x0   /* next token is an argument */
#define ST_INFILE   0x1   /* next token is the input file */
#define ST_OUTFILE  0x2   /* next token is the output file */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE_TSH];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE_TSH];  /* command line */
};
struct job_t job_list[MAXJOBS]; /* The job list */

struct cmdline_tokens {
    int argc;               /* Number of arguments */
    char *argv[MAXARGS];    /* The arguments list */
    char *infile;           /* The input file */
    char *outfile;          /* The output file */
    enum builtins_t {       /* Indicates if argv[0] is a builtin command */
        BUILTIN_NONE,
        BUILTIN_QUIT,
        BUILTIN_JOBS,
        BUILTIN_BG,
        BUILTIN_FG} builtins;
};

/* End global variables */

/* Function prototypes */
void eval(char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, struct cmdline_tokens *tok); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *job_list);
int maxjid(struct job_t *job_list); 
int addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *job_list, pid_t pid); 
pid_t fgpid(struct job_t *job_list);
struct job_t *getjobpid(struct job_t *job_list, pid_t pid);
struct job_t *getjobjid(struct job_t *job_list, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *job_list, int output_fd);

void usage(void);


/*
 * main - The shell's main routine 
 */
int 
main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE_TSH];    /* cmdline for fgets */
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
        case 'h':             /* print help message */
            usage();
            break;
        case 'v':             /* emit additional diagnostic info */
            verbose = 1;
            break;
        case 'p':             /* don't print a prompt */
            emit_prompt = 0;  /* handy for automatic testing */
            break;
        default:
            usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */
    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(job_list);

    /* Execute the shell's read/eval loop */
    while (1) {

        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { 
            /* End of file (ctrl-d) */
            printf ("\n");
            fflush(stdout);
            fflush(stderr);
            exit(0);
        }
        
        /* Remove the trailing newline */
        cmdline[strlen(cmdline)-1] = '\0';
        
        /* Evaluate the command line */
        eval(cmdline);
        
        fflush(stdout);
        fflush(stdout);
    } 
    
    exit(0); /* control never reaches here */
}

/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
 */
void 
eval(char *cmdline) 
{
    int bg;              /* should the job run in bg or fg? */
    int jid;
    struct cmdline_tokens tok;
    struct job_t* job;
    pid_t pid;
    sigset_t mask, mask_all;
    sigset_t prev_all, fgmask;
    
    Sigfillset(&mask_all);
    Sigemptyset(&mask);

    //Sigaddset(&mask_one, SIGCHLD);

    /* Parse command line */
    bg = parseline(cmdline, &tok); 
    if (bg == -1) /* parsing error */
        return;
    if (tok.argv[0] == NULL) /* ignore empty lines */
        return;
    
    /* if it is not a build in command */
    if (tok.builtins == BUILTIN_NONE) 
    {
        /* First we should lock the SIGCHILD for both parent and child*/
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        if ((pid = Fork()) == 0)
        {
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
            /*Assign a new gid to every child process*/
            setpgid(0, 0);
            /*redirect to the out file or the in file*/
            if (tok.outfile)
            {
                int out = open(tok.outfile, O_CREAT| O_WRONLY);
                dup2(out, 1);
                close(out);
            }
            if (tok.infile)
            {
                int in = open(tok.infile, O_RDONLY| O_CREAT);
                dup2(in, 0);
                close(in);
            }
            /*execute the not-build in command */
            execve(tok.argv[0], tok.argv, environ);
        }
        /*this job is a foregroud job*/
        if (!bg) 
        {
            /*add the job to the joblist set the state FG*/
            addjob(job_list, pid, FG, cmdline);
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
            /*wait for it to terminate*/
            while (fgpid(job_list) != 0)
            {
                sigsuspend(&mask);
            }
        }
        /*this job is a foregroud job */
        else
        {
            /*add the job to the joblist set the state BG*/
            addjob(job_list, pid, BG, cmdline);
            Sio_puts("[");
            Sio_putl(pid2jid(pid));
            Sio_puts("] (");
            Sio_putl(pid);
            Sio_puts(") ");
            Sio_puts(cmdline);
            Sio_puts("\n");
            Sigprocmask(SIG_SETMASK, &prev_all, NULL);
        }
            
    }
    /* JOBS PRINT OUT ALL THE JOBS*/
    if (tok.builtins == BUILTIN_JOBS)
    {
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        /* if needed redirect to the output file*/
        if (tok.outfile)
        {
            int out1 = open(tok.outfile, O_CREAT| O_WRONLY);
            listjobs(job_list, out1);
            close(out1);
        }
        else 
        {
            listjobs(job_list, 1);
        }
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    /* QUIT*/
    if (tok.builtins == BUILTIN_QUIT)
    {
        exit(0);
    }
    /* TRUN IT TO BG*/
    if (tok.builtins == BUILTIN_BG)
    {
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        char c = tok.argv[1][0];
        /*to check whether it is jid or pid*/
        if ( c == '%')
        {
            jid = atoi(tok.argv[1]+1);
        }
        else
        {
            jid = pid2jid(atoi(tok.argv[1]));
        }
        job = getjobjid(job_list,jid);
        job->state = BG;
        Sio_puts("[");
        Sio_putl(jid);
        Sio_puts("] (");
        Sio_putl(job->pid);
        Sio_puts(") ");
        Sio_puts(job->cmdline);
        Sio_puts("\n");
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);

    }
    /* TRUN IT TO FG*/
    if (tok.builtins == BUILTIN_FG)
    {
        Sigemptyset(&fgmask);
        Sigprocmask(SIG_BLOCK, &mask_all, &prev_all); 
        char c = tok.argv[1][0];
        /*to check whether it is jid or pid*/
        if ( c == '%') 
        {
            jid = atoi(tok.argv[1]+1);
        }
        else
        {
            jid = pid2jid(atoi(tok.argv[1]));
        }
        job = getjobjid(job_list,jid);
        /* Change the job state and send to SIGCONT to the -pid*/
        job->state = FG;
        kill(-(job->pid),SIGCONT);
        while (fgpid(job_list) != 0)
        {
            sigsuspend(&fgmask);
        }
        Sigprocmask(SIG_SETMASK, &prev_all, NULL);
    }
    return;
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Parameters:
 *   cmdline:  The command line, in the form:
 *
 *                command [arguments...] [< infile] [> oufile] [&]
 *
 *   tok:      Pointer to a cmdline_tokens structure. The elements of this
 *             structure will be populated with the parsed tokens. Characters 
 *             enclosed in single or double quotes are treated as a single
 *             argument. 
 * Returns:
 *   1:        if the user has requested a BG job
 *   0:        if the user has requested a FG job  
 *  -1:        if cmdline is incorrectly formatted
 * 
 * Note:       The string elements of tok (e.g., argv[], infile, outfile) 
 *             are statically allocated inside parseline() and will be 
 *             overwritten the next time this function is invoked.
 */
int 
parseline(const char *cmdline, struct cmdline_tokens *tok) 
{

    static char array[MAXLINE_TSH];          /* holds local copy of command line */
    const char delims[10] = " \t\r\n";   /* argument delimiters (white-space) */
    char *buf = array;                   /* ptr that traverses command line */
    char *next;                          /* ptr to the end of the current arg */
    char *endbuf;                        /* ptr to end of cmdline string */
    int is_bg;                           /* background job? */

    int parsing_state;                   /* indicates if the next token is the
                                            input or output file */

    if (cmdline == NULL) {
        (void) fprintf(stderr, "Error: command line is NULL\n");
        return -1;
    }

    (void) strncpy(buf, cmdline, MAXLINE_TSH);
    endbuf = buf + strlen(buf);

    tok->infile = NULL;
    tok->outfile = NULL;

    /* Build the argv list */
    parsing_state = ST_NORMAL;
    tok->argc = 0;

    while (buf < endbuf) {
        /* Skip the white-spaces */
        buf += strspn (buf, delims);
        if (buf >= endbuf) break;

        /* Check for I/O redirection specifiers */
        if (*buf == '<') {
            if (tok->infile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_INFILE;
            buf++;
            continue;
        }
        if (*buf == '>') {
            if (tok->outfile) {
                (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
                return -1;
            }
            parsing_state |= ST_OUTFILE;
            buf ++;
            continue;
        }

        if (*buf == '\'' || *buf == '\"') {
            /* Detect quoted tokens */
            buf++;
            next = strchr (buf, *(buf-1));
        } else {
            /* Find next delimiter */
            next = buf + strcspn (buf, delims);
        }
        
        if (next == NULL) {
            /* Returned by strchr(); this means that the closing
               quote was not found. */
            (void) fprintf (stderr, "Error: unmatched %c.\n", *(buf-1));
            return -1;
        }

        /* Terminate the token */
        *next = '\0';

        /* Record the token as either the next argument or the i/o file */
        switch (parsing_state) {
        case ST_NORMAL:
            tok->argv[tok->argc++] = buf;
            break;
        case ST_INFILE:
            tok->infile = buf;
            break;
        case ST_OUTFILE:
            tok->outfile = buf;
            break;
        default:
            (void) fprintf(stderr, "Error: Ambiguous I/O redirection\n");
            return -1;
        }
        parsing_state = ST_NORMAL;

        /* Check if argv is full */
        if (tok->argc >= MAXARGS-1) break;

        buf = next + 1;
    }

    if (parsing_state != ST_NORMAL) {
        (void) fprintf(stderr,
                       "Error: must provide file name for redirection\n");
        return -1;
    }

    /* The argument list must end with a NULL pointer */
    tok->argv[tok->argc] = NULL;

    if (tok->argc == 0)  /* ignore blank line */
        return 1;

    if (!strcmp(tok->argv[0], "quit")) {                 /* quit command */
        tok->builtins = BUILTIN_QUIT;
    } else if (!strcmp(tok->argv[0], "jobs")) {          /* jobs command */
        tok->builtins = BUILTIN_JOBS;
    } else if (!strcmp(tok->argv[0], "bg")) {            /* bg command */
        tok->builtins = BUILTIN_BG;
    } else if (!strcmp(tok->argv[0], "fg")) {            /* fg command */
        tok->builtins = BUILTIN_FG;
    } else {
        tok->builtins = BUILTIN_NONE;
    }

    /* Should the job run in the background? */
    if ((is_bg = (*tok->argv[tok->argc-1] == '&')) != 0)
        tok->argv[--tok->argc] = NULL;

    return is_bg;
}


/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP, SIGTSTP, SIGTTIN or SIGTTOU signal. The 
 *     handler reaps all available zombie children, but doesn't wait 
 *     for any other currently running children to terminate.  
 */
void 
sigchld_handler(int sig) 
{
    int status;
    pid_t pid;
    sigset_t mask_all, prev_all;

    Sigemptyset(&mask_all);

    Sigaddset(&mask_all, SIGCHLD);
    Sigaddset(&mask_all, SIGINT);
    Sigaddset(&mask_all, SIGTSTP);

   
   while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED) )> 0) {
          Sigprocmask(SIG_BLOCK, &mask_all, &prev_all);
        /*normally terminated*/
        if (WIFEXITED(status)) {
          deletejob(job_list, pid);
        }
        /*it is terminated by signal not caught*/
        if (WIFSIGNALED(status)) {
            Sio_puts("Job [");
            Sio_putl(pid2jid(pid));
            Sio_puts("] (");
            Sio_putl(pid);
            Sio_puts(") terminated by signal ");
            Sio_putl(WTERMSIG(status));
            Sio_puts("\n");
            deletejob(job_list, pid); 
        } 
        /*the job causes the returns  is currently stopped*/
        if (WIFSTOPPED(status)) 
        {
            struct job_t* job = getjobpid(job_list,pid);
            job->state = ST;
            Sio_puts("Job [");
            Sio_putl(pid2jid(pid));
            Sio_puts("] (");
            Sio_putl(pid);
            Sio_puts(") stopped by signal ");
            Sio_putl(WSTOPSIG(status));
            Sio_puts("\n");
        } 
        Sigprocmask(SIG_SETMASK,&prev_all, NULL);
        
    }

    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void 
sigint_handler(int sig) 
{
    sigset_t mask_all, pre_all;
    Sigfillset(&mask_all);
    /*get the foregroud job by using the function fgpid.*/
    pid_t pid = fgpid(job_list);
    /*only kill the job in the foreground.*/
    if (pid != 0) 
    {
        Sigprocmask(SIG_BLOCK, &mask_all, &pre_all);
        kill(-pid, SIGINT);
        Sigprocmask(SIG_SETMASK, &pre_all, NULL);
    }
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void 
sigtstp_handler(int sig) 
{
    sigset_t mask_all, pre_all;
    Sigfillset(&mask_all);
    /*get the foregroud job by using the function fgpid.*/
    pid_t pid = fgpid(job_list);
    /*only stop the job in the foreground.*/
    if (pid != 0) 
    {
        Sigprocmask(SIG_BLOCK, &mask_all, &pre_all);
        kill(-pid, SIGTSTP);
        Sigprocmask(SIG_SETMASK, &pre_all, NULL);
    }
    return;
}

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void 
sigquit_handler(int sig) 
{
    sio_error("Terminating after receipt of SIGQUIT signal\n");
}



/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void 
clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void 
initjobs(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        clearjob(&job_list[i]);
}

/* maxjid - Returns largest allocated job ID */
int 
maxjid(struct job_t *job_list) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid > max)
            max = job_list[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int 
addjob(struct job_t *job_list, pid_t pid, int state, char *cmdline) 
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == 0) {
            job_list[i].pid = pid;
            job_list[i].state = state;
            job_list[i].jid = nextjid++;
            if (nextjid > MAXJOBS)
                nextjid = 1;
            strcpy(job_list[i].cmdline, cmdline);
            if(verbose){
                printf("Added job [%d] %d %s\n",
                       job_list[i].jid,
                       job_list[i].pid,
                       job_list[i].cmdline);
            }
            return 1;
        }
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int 
deletejob(struct job_t *job_list, pid_t pid) 
{
    int i;

    if (pid < 1)
        return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (job_list[i].pid == pid) {
            clearjob(&job_list[i]);
            nextjid = maxjid(job_list)+1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t 
fgpid(struct job_t *job_list) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].state == FG)
            return job_list[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t 
*getjobpid(struct job_t *job_list, pid_t pid) {
    int i;

    if (pid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid)
            return &job_list[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *job_list, int jid) 
{
    int i;

    if (jid < 1)
        return NULL;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].jid == jid)
            return &job_list[i];
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int 
pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
        return 0;
    for (i = 0; i < MAXJOBS; i++)
        if (job_list[i].pid == pid) {
            return job_list[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void 
listjobs(struct job_t *job_list, int output_fd) 
{
    int i;
    char buf[MAXLINE_TSH];

    for (i = 0; i < MAXJOBS; i++) {
        memset(buf, '\0', MAXLINE_TSH);
        if (job_list[i].pid != 0) {
            sprintf(buf, "[%d] (%d) ", job_list[i].jid, job_list[i].pid);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE_TSH);
            switch (job_list[i].state) {
            case BG:
                sprintf(buf, "Running    ");
                break;
            case FG:
                sprintf(buf, "Foreground ");
                break;
            case ST:
                sprintf(buf, "Stopped    ");
                break;
            default:
                sprintf(buf, "listjobs: Internal error: job[%d].state=%d ",
                        i, job_list[i].state);
            }
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
            memset(buf, '\0', MAXLINE_TSH);
            sprintf(buf, "%s\n", job_list[i].cmdline);
            if(write(output_fd, buf, strlen(buf)) < 0) {
                fprintf(stderr, "Error writing to output file\n");
                exit(1);
            }
        }
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void 
usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}
