#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "parser/parse.h"
#include "ish.h"

static pid_t exec_foreground_job(job* curr_job, jobs_manager* jobm, char* envp[]);
static pid_t exec_background_job(job* curr_job, jobs_manager* jobm, char* envp[]);
static int exec_pipe_process(process* curr_process, char* envp[]);
static int exec_process(process* curr_process, char* envp[]);
static int execve_path(char pname[], char* arglist[], char *envp[]);

pid_t exec_job(job* curr_job, jobs_manager* jobm, char* envp[]){
	int ret;
	if (curr_job == NULL){
		return -1;
	}
	if (curr_job->mode == FOREGROUND){
		ret = exec_foreground_job(curr_job, jobm, envp);
		return ret;
	}
	else{
		ret = exec_background_job(curr_job, jobm, envp);
		return ret;
	}
}

pid_t exec_foreground_job(job* curr_job, jobs_manager* jobm, char* envp[]){
	pid_t pid;
	int status;
	if ((pid = fork()) == 0){
		exec_pipe_process(curr_job->process_list, envp);
		perror("exec_pipe_process");
		exit(EXIT_FAILURE);
	}
	else{
		setpgid(pid, pid);
		tcsetpgrp(0, pid);
		jobm->fg_job.status = RUNNING;
		jobm->fg_job.pid = pid;
		jobm->fg_job.job_data = curr_job;
		waitpid(pid, &status, WUNTRACED);
		tcsetpgrp(0, getpgrp());		// これ自体は実行されないが、SIGTTOUシグナルが生成される。
		if (WIFEXITED(status)){
			jobm->fg_job.status = DONE;
			jobm->fg_job.pid = 0;
			free(jobm->fg_job.job_data);
		}
		if (WIFSTOPPED(status)){
			jobm->bg_jobs[jobm->tail].status = STOPPED;
			jobm->bg_jobs[jobm->tail].pid = pid;
			jobm->bg_jobs[jobm->tail].job_data = curr_job;
			jobm->bg_jobs[jobm->tail].job_data->mode = BACKGROUND;
			int i;
			printf("\n[%d]\tStopped\t\t", jobm->tail+1);
			for (i = 0; jobm->bg_jobs[jobm->tail].job_data->process_list->argument_list[i] != NULL; i++)
				printf("%s ", jobm->bg_jobs[jobm->tail].job_data->process_list->argument_list[i]);
			printf("\n");
			jobm->tail = (jobm->tail < JOBS_MAX - 1) ? jobm->tail + 1 : JOBS_MAX;
		}
		return 0;
	}
}

pid_t exec_background_job(job* curr_job, jobs_manager* jobm, char* envp[]){
	pid_t pid;
	if ((pid = fork()) == 0){
		setpgid(0, 0);
		exec_pipe_process(curr_job->process_list, envp);
		perror("exec_pipe_process");
		exit(EXIT_FAILURE);
	}
	else{
		jobm->bg_jobs[jobm->tail].status = RUNNING;
		jobm->bg_jobs[jobm->tail].pid = pid;
		jobm->bg_jobs[jobm->tail].job_data = curr_job;
		jobm->tail = (jobm->tail < JOBS_MAX - 1) ? jobm->tail + 1 : JOBS_MAX;
		printf("[%d] %u\n", jobm->tail, pid);
		return pid;
	}
}

static int exec_pipe_process(process* curr_process, char* envp[]){
	pid_t pid[2];
	int fd[2];
	int status;

	if (curr_process->next != NULL){
		if (pipe(fd) < 0){
			perror("pipe");
			exit(EXIT_FAILURE);
		}
		if ((pid[0] = fork()) == 0){
			close(fd[1]);
			dup2(fd[0], 0);
			exec_pipe_process(curr_process->next, envp);
			exit(EXIT_SUCCESS);
		}
		else if ((pid[1] = fork()) == 0){
			close(fd[0]);
			dup2(fd[1], 1);
			exec_process(curr_process, envp);
			perror("exec_process");
			exit(EXIT_FAILURE);
		}
		else{
			close(fd[0]);
			close(fd[1]);
			while (waitpid(-1, &status, WUNTRACED) < 0);
			while (waitpid(-1, &status, WUNTRACED) < 0);			// 2つのプロセスが正常終了するまで待つ
			exit(EXIT_SUCCESS);
		}
	}
	else{
		if ((pid[0] = fork()) == 0){
			exec_process(curr_process, envp);
			perror("exec_process");
			exit(EXIT_FAILURE);
		}
		else{
			while(waitpid(pid[0], &status, WUNTRACED) < 0);		// プロセスが正常終了するまで待つ
			exit(EXIT_SUCCESS);
		}
	}
}

static int exec_process(process* curr_process, char* envp[]){
	int ifd = 0, ofd = 1;
	if (curr_process->input_redirection != NULL){
		if ((ifd = open(curr_process->input_redirection, O_RDONLY | O_CLOEXEC)) < 0){
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(ifd, 0);
	}
	if (curr_process->output_redirection != NULL){
		if (curr_process->output_option == TRUNC && (ofd = open(curr_process->output_redirection, O_WRONLY | O_CLOEXEC | O_CREAT | O_TRUNC, 0666)) < 0){
			perror("open");
			exit(EXIT_FAILURE);
		}
		if (curr_process->output_option == APPEND && (ofd = open(curr_process->output_redirection, O_WRONLY | O_CLOEXEC | O_CREAT | O_APPEND, 0666)) < 0){
			perror("open");
			exit(EXIT_FAILURE);
		}
		dup2(ofd, 1);
	}
	execve_path(curr_process->program_name, curr_process->argument_list, envp);
	perror("execve");
	exit(EXIT_FAILURE);
}

static int execve_path(char pname[], char* arglist[], char *envp[]){
	execve(pname, arglist, envp);
	int i, pathlen;
	char *path = "/bin", *tok;
	char route[ROUTELEN];
	
	for (i = 0; envp[i] != NULL; i++){
		if (strncmp(envp[i], "PATH=", 5) == 0)
			path = envp[i] + 5;
	}
	tok = strtok(path, ":");
	strcpy(route, tok);
	pathlen = strlen(route);
	if (route[pathlen - 1] != '/'){
		route[pathlen] = '/';
		route[pathlen + 1] = '\0';
	}
	strcat(route, pname);
	execve(route, arglist, envp);
	if (errno != ENOENT)
		return -1;
	while (tok != NULL){
		tok = strtok(NULL, ":");
		if (tok != NULL){
			strcpy(route, tok);
			pathlen = strlen(route);
			if (route[pathlen - 1] != '/'){
				route[pathlen] = '/';
				route[pathlen + 1] = '\0';
			}
			strcat(route, pname);
			execve(route, arglist, envp);
			if (errno != ENOENT)
				return -1;
		}
	}
	return -1;
}

