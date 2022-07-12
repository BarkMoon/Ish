#ifndef __ISH_H__
#define __ISH_H__

#define ROUTELEN 256
#define FD_MAX 256
#define PIPE_PROCESS_MAX 8
#define JOBS_MAX 1024

typedef enum{
	FREE, RUNNING, STOPPED, DONE
}job_status;

typedef struct{
	job_status status;
	pid_t pid;
	job *job_data;
}job_info;

typedef struct{
	int tail;
	job_info fg_job;
	job_info bg_jobs[JOBS_MAX];
}jobs_manager;

pid_t exec_job(job* curr_job, jobs_manager* jobm, char* envp[]);
void init_jobs_manager(jobs_manager *jobm);
void check_bg_jobs(jobs_manager *jobm);
void resume_bg_job(jobs_manager *jobm);
void print_bg_jobs(jobs_manager *jobm);

#endif

