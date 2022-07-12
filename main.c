#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser/parse.h"
#include "ish.h"

void handler(int signal);

int status;
jobs_manager jobm;

pid_t ish_pid;

int main(int argc, char* argv[], char* envp[]) {
    char s[LINELEN];
    job *curr_job;
	struct sigaction sigact;

	ish_pid = getpid();
	init_jobs_manager(&jobm);
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = handler;
	if (sigaction(SIGINT, &sigact, NULL) < 0){
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGTSTP, &sigact, NULL) < 0){
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGCHLD, &sigact, NULL) < 0){
		perror("sigaction");
		exit(EXIT_FAILURE);
	}
	if (sigaction(SIGTTOU, &sigact, NULL) < 0){
		perror("sigaction");
		exit(EXIT_FAILURE);
	}

    while(get_line(s, LINELEN)) {
        if (!strcmp(s, "exit\n"))
            break;
		else if (!strcmp(s, "bg\n")){
			resume_bg_job(&jobm);
		}
		else if (!strcmp(s, "jobs\n")){
			print_bg_jobs(&jobm);
		}
		else if (!strncmp(s, "cd", 2)){
			if (strlen(s) <= 4){
				if (chdir(getenv("HOME")) != 0) {
      				perror("cd");
    			}
			}
			else{
				char *path = &s[3];
				if (path[strlen(path) - 1] == '\n'){
					path[strlen(path) - 1] = '\0';
				}
				if (chdir(path) != 0) {
      				perror("cd");
    			}
			}
		}
		else{
			curr_job = parse_line(s);
			exec_job(curr_job, &jobm, envp);
		}
		check_bg_jobs(&jobm);
    }
		return 0;
}

void handler(int signal){
	pid_t pid;
	switch (signal){
		case SIGINT:
			if(getpid() != ish_pid)
				kill(0, SIGTERM);
			break;
		case SIGTSTP:
			if (getpid() != ish_pid)
				kill(0, SIGSTOP);
			break;
		case SIGCHLD:
			while ((pid = waitpid(-1, &status, WNOHANG)) > 0){
				int i;
				for (i = 0; i < jobm.tail; i++)
					if (pid == jobm.bg_jobs[i].pid){
						jobm.bg_jobs[i].status = DONE;
						break;
					}
			}
			break;
		case SIGTTOU:
			tcsetpgrp(0, getpgrp());
			break;
	}
}