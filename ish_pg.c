#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include "parser/parse.h"
#include "ish.h"

void print_job_list(job*);

void init_jobs_manager(jobs_manager *jobm){
	int i;
	jobm->tail = 0;
	jobm->fg_job.status = FREE;
	jobm->fg_job.pid = 0;
	jobm->fg_job.job_data = NULL;
	for (i = 0; i < JOBS_MAX; i++){
		jobm->bg_jobs[i].status = FREE;
		jobm->bg_jobs[i].pid = 0;
		jobm->bg_jobs[i].job_data = NULL;
	}
}

void check_bg_jobs(jobs_manager *jobm){
	int i, j, newtail = 0;
	for (i = 0; i < jobm->tail; i++){
		if (jobm->bg_jobs[i].status == DONE){
			printf("[%d]\tDone\t\t", i + 1);
			for (j = 0; jobm->bg_jobs[i].job_data->process_list->argument_list[j] != NULL; j++)
				printf("%s ", jobm->bg_jobs[i].job_data->process_list->argument_list[j]);
			printf("\n");
			jobm->bg_jobs[i].status = FREE;
			jobm->bg_jobs[i].pid = 0;
			free_job(jobm->bg_jobs[i].job_data);
		}
		else if(jobm->bg_jobs[i].status != FREE)
			newtail = (i < JOBS_MAX - 1) ? i + 1 : JOBS_MAX;
	}
	jobm->tail = newtail;
}

void resume_bg_job(jobs_manager *jobm){
	int i;
	for (i = 0; i < jobm->tail; i++){
		if (jobm->bg_jobs[i].status == STOPPED){
			jobm->bg_jobs[i].status = RUNNING;
			printf("[%d]\tContinue\n", i+1);
			kill(-jobm->bg_jobs[i].pid, SIGCONT);
			break;
		}
	}
}

void print_bg_jobs(jobs_manager *jobm){
	int i;
	for (i = 0; i < jobm->tail; i++){
		if (jobm->bg_jobs[i].status == RUNNING || jobm->bg_jobs[i].status == STOPPED){
			printf("[%d] status: %d, pid: %u\n", i+1, jobm->bg_jobs[i].status, jobm->bg_jobs[i].pid);
			print_job_list(jobm->bg_jobs[i].job_data);
		}
	}
}

