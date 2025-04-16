/**
 * File: sim_offline_edfvd.c
 * Thoroughly reviewed offline EDF-VD example with debug prints.
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <math.h>
 #include <string.h>
 #include <unistd.h>    // for getcwd debug
 #include <limits.h>    // for PATH_MAX
 #include "FreeRTOS.h"  // If you want, but not strictly necessary for offline code
 #include "sim_offline_edfvd.h"
 
 #define MAX_TASKS 50
 #define MAX_JOBS  5000
 
 typedef enum { CRIT_LOW = 0, CRIT_HIGH } CritLevel_t;
 
 typedef struct {
     char  name[32];
     double phase;
     double period;
     double wcet;
     double deadline;
     CritLevel_t critLevel;
     double virtualDeadline;
     int jobCount;
 } TaskInfo_t;
 
 typedef struct {
     int     taskIndex;
     int     jobId;
     double  arrivalTime;
     double  absoluteDeadline;
     double  virtualDeadline;
     double  wcet;
     double  actualExecTime;
     double  remainingTime;
     double  startTime;
     double  finishTime;
     int     finished;
 } Job_t;
 
 /* Global arrays, static file scope */
 static TaskInfo_t tasks[MAX_TASKS];
 static Job_t      jobs[MAX_JOBS];
 
 /* Global counters */
 static int g_numTasks = 0;
 static int g_numJobs  = 0;
 
 /* local function prototypes */
 static void parseTaskFile(const char* filename);
 static void computeHyperPeriodAndJobCounts(double* hyperPeriod);
 static void computeEDFVDParameters(void);
 static void buildJobsArray(double hyperPeriod, const char* execTimesFile);
 static void scheduleEDFVD(double hyperPeriod);
 static void writeScheduleToFile(const char* schedFile);
 static void analyzeSchedule(const char* analysisFile);
 
 /* For capturing scheduling slices. */
 typedef struct {
     double start;
     double end;
     int    taskIndex;
     int    jobId;
 } Slice_t;
 
 static Slice_t slices[10000];
 static int g_numSlices = 0;
 
 /* gcd/lcm helpers */
 static long long gcdLL(long long a, long long b)
 {
     if(b == 0) return a;
     return gcdLL(b, a % b);
 }
 static long long lcmLL(long long a, long long b)
 {
     long long g = gcdLL(a,b);
     if(g == 0) return 0;
     return (a / g) * b;
 }
 
 /*-----------------------------------------------------------
  * The main entry point for the offline simulation
  * (called from the FreeRTOS simulation task).
  *-----------------------------------------------------------*/
 void vRunOfflineEDFVD(void)
{
    g_numTasks = 0;
    g_numJobs  = 0;
    g_numSlices = 0;
  
    // Debug: print working directory
    char cwd[PATH_MAX];
    if(getcwd(cwd, sizeof(cwd)) != NULL)
        printf("DEBUG: Current working directory: %s\n", cwd);
    else
        perror("DEBUG: getcwd() error");

    const char* taskFile      = "tasks.txt";
    const char* execTimesFile = "exec_times.txt";
    const char* scheduleOut   = "schedule_output.txt";
    const char* analysisOut   = "schedule_analysis.txt";

    printf("DEBUG: Reading tasks from %s...\n", taskFile);
    parseTaskFile(taskFile);
    printf("DEBUG: After parseTaskFile, g_numTasks = %d\n", g_numTasks);
    if (g_numTasks <= 0) {
        printf("DEBUG: No tasks parsed. Exiting simulation.\n");
        return;
    }

    double hyperPeriod = 0;
    computeHyperPeriodAndJobCounts(&hyperPeriod);
    printf("DEBUG: Computed HyperPeriod = %.2f\n", hyperPeriod);

    computeEDFVDParameters();

    printf("DEBUG: Building jobs array from %s...\n", execTimesFile);
    buildJobsArray(hyperPeriod, execTimesFile);
    printf("DEBUG: Total jobs built = %d\n", g_numJobs);
    if (g_numJobs <= 0) {
        printf("DEBUG: No jobs built. Exiting simulation.\n");
        return;
    }

    printf("DEBUG: Starting scheduleEDFVD...\n");
    scheduleEDFVD(hyperPeriod);

    printf("DEBUG: Writing schedule to %s...\n", scheduleOut);
    writeScheduleToFile(scheduleOut);
    printf("DEBUG: Analyzing schedule and writing results to %s...\n", analysisOut);
    analyzeSchedule(analysisOut);

    printf("DEBUG: Offline EDF-VD simulation completed. Results in %s and %s.\n", scheduleOut, analysisOut);
}

 
 /*-----------------------------------------------------------
  * parseTaskFile
  *-----------------------------------------------------------*/
 static void parseTaskFile(const char* filename)
 {
     FILE* fp = fopen(filename, "r");
     if(!fp){
         printf("ERROR: Cannot open %s.\n", filename);
         return;
     } else {
         printf("DEBUG: Successfully opened %s.\n", filename);
     }
 
     if(fscanf(fp, "%d", &g_numTasks) != 1) {
         printf("ERROR: Failed to read number of tasks from %s.\n", filename);
         g_numTasks = 0;
         fclose(fp);
         return;
     }
     printf("DEBUG: parseTaskFile => Read g_numTasks = %d\n", g_numTasks);
 
     for(int i=0; i<g_numTasks; i++){
         char c;
         /* Example line: T1 0 10 3 10 H */
         int ret = fscanf(fp, "%s %lf %lf %lf %lf %c",
                          tasks[i].name,
                          &tasks[i].phase,
                          &tasks[i].period,
                          &tasks[i].wcet,
                          &tasks[i].deadline,
                          &c);
         if(ret != 6) {
             printf("ERROR: Malformed task line %d in %s. ret=%d\n", i, filename, ret);
             g_numTasks = i; // only i tasks read so far
             break;
         }
 
         tasks[i].critLevel = (c=='H' || c=='h') ? CRIT_HIGH : CRIT_LOW;
         tasks[i].virtualDeadline = tasks[i].deadline; /* Will be scaled for high crit if needed */
         tasks[i].jobCount = 0;
 
         printf("DEBUG: Task[%d]: name=%s, phase=%.2f, period=%.2f, wcet=%.2f, deadline=%.2f, crit=%c\n",
                i, tasks[i].name, tasks[i].phase, tasks[i].period, tasks[i].wcet, tasks[i].deadline, c);
     }
 
     fclose(fp);
 }
 
 /*-----------------------------------------------------------
  * computeHyperPeriodAndJobCounts
  *-----------------------------------------------------------*/
 static void computeHyperPeriodAndJobCounts(double* hyperPeriod)
 {
     *hyperPeriod = 1.0; /* Default if something fails, we keep 1 */
 
     long long arr[MAX_TASKS];
     int validCount = (g_numTasks > MAX_TASKS) ? MAX_TASKS : g_numTasks;
     long long l = 1;
 
     /* If g_numTasks=0, we skip. */
     if(validCount == 0) {
         printf("DEBUG: No tasks, skipping hyperperiod calc.\n");
         return;
     }
 
     for(int i=0; i<validCount; i++){
         long long p_ll = (long long) (tasks[i].period + 0.5); /* Round if needed */
         if(p_ll <= 0) p_ll = 1; /* avoid zero or negative */
         arr[i] = p_ll;
     }
 
     for(int i=0; i<validCount; i++){
         l = lcmLL(l, arr[i]);
     }
     *hyperPeriod = (double) l;
 
     printf("DEBUG: computed LCM hyperPeriod = %.2f\n", *hyperPeriod);
 
     /* Now compute how many jobs each task will have within [0..hyperPeriod). */
     for(int i=0; i<validCount; i++){
         double HP = *hyperPeriod;
         double ph = tasks[i].phase;
         double pd = tasks[i].period;
         if(ph >= HP) {
             tasks[i].jobCount = 0;
             continue;
         }
         /* # of arrivals in [0..HP): e.g. floor((HP - phase)/period) */
         int count = (int) floor((HP - ph) / pd);
         tasks[i].jobCount = (count < 0) ? 0 : count;
         printf("DEBUG: Task[%d]=%s => jobCount=%d\n", i, tasks[i].name, tasks[i].jobCount);
     }
 }
 
 /*-----------------------------------------------------------
  * computeEDFVDParameters
  *-----------------------------------------------------------*/
 static void computeEDFVDParameters(void)
 {
     double U_H = 0.0, U_L = 0.0;
     for(int i=0; i<g_numTasks; i++){
         double util = tasks[i].wcet / tasks[i].period;
         if(tasks[i].critLevel == CRIT_HIGH) {
             U_H += util;
         } else {
             U_L += util;
         }
     }
     printf("DEBUG: U_H=%.2f, U_L=%.2f\n", U_H, U_L);
 
     double x = 1.0;
     if(U_L < 1.0){
         x = U_H / (1.0 - U_L);
         if(x > 1.0) x = 1.0;
     }
     printf("DEBUG: scaling factor x=%.2f\n", x);
 
     for(int i=0; i<g_numTasks; i++){
         if(tasks[i].critLevel == CRIT_HIGH){
             tasks[i].virtualDeadline = tasks[i].deadline * x;
             printf("DEBUG: Task[%d]=%s => scaled vDL=%.2f\n", i, tasks[i].name, tasks[i].virtualDeadline);
         }
     }
 }
 
 /*-----------------------------------------------------------
  * buildJobsArray
  *-----------------------------------------------------------*/
 static void buildJobsArray(double hyperPeriod, const char* execTimesFile)
 {
     FILE* fp = fopen(execTimesFile, "r");
     if(!fp){
         printf("ERROR: Cannot open %s.\n", execTimesFile);
         return;
     } else {
         printf("DEBUG: Successfully opened %s.\n", execTimesFile);
     }
 
     g_numJobs = 0;
 
     for(int t=0; t<g_numTasks; t++){
         int count = tasks[t].jobCount;
         printf("DEBUG: building jobs for Task[%d]=%s => jobCount=%d\n", t, tasks[t].name, count);
 
         if(count <= 0) {
             /* skip reading actual exec times for this task */
             continue;
         }
 
         double* actualETs = (double*) malloc(sizeof(double) * count);
         if(!actualETs) {
             printf("ERROR: malloc failed for actualETs.\n");
             fclose(fp);
             return;
         }
 
         /* read 'count' actual times from file for this task line */
         for(int j=0; j<count; j++){
             int ret = fscanf(fp, "%lf", &actualETs[j]);
             if(ret != 1) {
                 printf("ERROR: exec_times.txt parse error reading times for Task[%d]=%s job %d. ret=%d\n", t, tasks[t].name, j, ret);
                 free(actualETs);
                 fclose(fp);
                 return;
             }
         }
 
         /* Now create each job structure */
         for(int j=0; j<count; j++){
             double arrival = tasks[t].phase + j * tasks[t].period;
             if(arrival >= hyperPeriod) {
                 /* skip if it doesn't start before HP */
                 continue;
             }
             jobs[g_numJobs].taskIndex       = t;
             jobs[g_numJobs].jobId           = j;
             jobs[g_numJobs].arrivalTime     = arrival;
             jobs[g_numJobs].wcet            = tasks[t].wcet;
             jobs[g_numJobs].actualExecTime  = actualETs[j];
             jobs[g_numJobs].remainingTime   = actualETs[j];
             jobs[g_numJobs].finished        = 0;
 
             double realDL = arrival + tasks[t].deadline;
             double vDL    = arrival + tasks[t].virtualDeadline;
             jobs[g_numJobs].absoluteDeadline = realDL;
             jobs[g_numJobs].virtualDeadline  = vDL;
 
             jobs[g_numJobs].startTime  = -1.0;
             jobs[g_numJobs].finishTime = -1.0;
 
             printf("DEBUG: Created Job[%d]: T=%d, jobId=%d, arrival=%.2f, exec=%.2f, realDL=%.2f, vDL=%.2f\n",
                    g_numJobs, t, j, arrival, actualETs[j], realDL, vDL);
 
             g_numJobs++;
             if(g_numJobs >= MAX_JOBS) {
                 printf("ERROR: Too many jobs, reached MAX_JOBS.\n");
                 free(actualETs);
                 fclose(fp);
                 return;
             }
         }
         free(actualETs);
     }
 
     fclose(fp);
 }
 
 /*-----------------------------------------------------------
  * scheduleEDFVD
  *-----------------------------------------------------------*/
 
 static int compareVDL(const void* a, const void* b)
 {
     const Job_t* ja = (const Job_t*)a;
     const Job_t* jb = (const Job_t*)b;
     if(ja->virtualDeadline < jb->virtualDeadline) return -1;
     else if(ja->virtualDeadline > jb->virtualDeadline) return 1;
     else return 0;
 }
 
 void scheduleEDFVD(double hyperPeriod)
 {
     double now = 0.0;
     g_numSlices = 0;
     int lastJobIndex = -1;
 
     printf("DEBUG: Entering scheduleEDFVD with hyperPeriod=%.2f\n", hyperPeriod);
 
     while(now < hyperPeriod)
     {
         printf("DEBUG: scheduling loop, now=%.2f\n", now);
 
         /* 1) find active jobs: arrived, not finished, and has remaining time */
         int activeIndices[2000];
         int activeCount = 0;
         for(int i=0; i<g_numJobs; i++){
             if(!jobs[i].finished &&
                jobs[i].arrivalTime <= now &&
                jobs[i].remainingTime > 0.0)
             {
                 activeIndices[activeCount++] = i;
             }
         }
 
         printf("DEBUG: activeCount=%d\n", activeCount);
 
         if(activeCount == 0) {
             /* no active job => jump to next arrival */
             double nextArrival = hyperPeriod;
             for(int i=0; i<g_numJobs; i++){
                 if(!jobs[i].finished && jobs[i].arrivalTime > now){
                     if(jobs[i].arrivalTime < nextArrival){
                         nextArrival = jobs[i].arrivalTime;
                     }
                 }
             }
             printf("DEBUG: nextArrival=%.2f\n", nextArrival);
 
             if(nextArrival > now && nextArrival < hyperPeriod){
                 now = nextArrival;
                 continue; 
             } else {
                 printf("DEBUG: no more arrivals or nextArrival >= hyperPeriod => break\n");
                 break;
             }
         }
 
         /* 2) among active, pick earliest VDL (if >1 active) */
         if(activeCount > 1){
             Job_t tmp[2000];
             for(int i=0; i<activeCount; i++){
                 tmp[i] = jobs[activeIndices[i]];
             }
             qsort(tmp, activeCount, sizeof(Job_t), compareVDL);
 
             /* The first in tmp is earliest. Find which one in 'jobs' it matches. */
             int chosenIndex = -1;
             for(int i=0; i<activeCount; i++){
                 if( jobs[activeIndices[i]].virtualDeadline == tmp[0].virtualDeadline &&
                     jobs[activeIndices[i]].taskIndex == tmp[0].taskIndex &&
                     jobs[activeIndices[i]].jobId == tmp[0].jobId )
                 {
                     chosenIndex = activeIndices[i];
                     break;
                 }
             }
             if(chosenIndex < 0) {
                 printf("ERROR: could not match chosen job.\n");
                 break;
             }
 
             /* next completion time or next arrival => next decision. */
             double remain = jobs[chosenIndex].remainingTime;
             double nextArrival = hyperPeriod;
             for(int k=0; k<g_numJobs; k++){
                 if(!jobs[k].finished && jobs[k].arrivalTime > now){
                     if(jobs[k].arrivalTime < nextArrival){
                         nextArrival = jobs[k].arrivalTime;
                     }
                 }
             }
             double finishIfUninterrupted = now + remain;
             double nextDecision = (nextArrival < finishIfUninterrupted) ? nextArrival : finishIfUninterrupted;
 
             /* If we changed job => new scheduling slice. */
             if(chosenIndex != lastJobIndex){
                 g_numSlices++;
                 if(g_numSlices >= 10000) {
                     printf("ERROR: slices array full.\n");
                     return;
                 }
                 slices[g_numSlices-1].start     = now;
                 slices[g_numSlices-1].taskIndex = jobs[chosenIndex].taskIndex;
                 slices[g_numSlices-1].jobId     = jobs[chosenIndex].jobId;
                 lastJobIndex = chosenIndex;
             }
 
             slices[g_numSlices-1].end = nextDecision;
 
             /* run chosen job from now to nextDecision */
             double delta = nextDecision - now;
             jobs[chosenIndex].remainingTime -= delta;
             if(jobs[chosenIndex].startTime < 0) {
                 jobs[chosenIndex].startTime = now;
             }
 
             now = nextDecision;
 
             /* if the job is now finished */
             if(jobs[chosenIndex].remainingTime <= 1e-9){
                 jobs[chosenIndex].finished = 1;
                 jobs[chosenIndex].finishTime = now;
                 printf("DEBUG: Job finished => T=%d, jobId=%d at time=%.2f\n",
                        jobs[chosenIndex].taskIndex, jobs[chosenIndex].jobId, now);
             }
         }
         else {
             /* Only one active job => simpler. */
             int chosenIndex = activeIndices[0];
             double remain = jobs[chosenIndex].remainingTime;
 
             double nextArrival = hyperPeriod;
             for(int k=0; k<g_numJobs; k++){
                 if(!jobs[k].finished && jobs[k].arrivalTime > now){
                     if(jobs[k].arrivalTime < nextArrival){
                         nextArrival = jobs[k].arrivalTime;
                     }
                 }
             }
             double finishIfUninterrupted = now + remain;
             double nextDecision = (nextArrival < finishIfUninterrupted) ? nextArrival : finishIfUninterrupted;
 
             if(chosenIndex != lastJobIndex){
                 g_numSlices++;
                 if(g_numSlices >= 10000) {
                     printf("ERROR: slices array full.\n");
                     return;
                 }
                 slices[g_numSlices-1].start     = now;
                 slices[g_numSlices-1].taskIndex = jobs[chosenIndex].taskIndex;
                 slices[g_numSlices-1].jobId     = jobs[chosenIndex].jobId;
                 lastJobIndex = chosenIndex;
             }
             slices[g_numSlices-1].end = nextDecision;
 
             double delta = nextDecision - now;
             jobs[chosenIndex].remainingTime -= delta;
             if(jobs[chosenIndex].startTime < 0) {
                 jobs[chosenIndex].startTime = now;
             }
 
             now = nextDecision;
 
             if(jobs[chosenIndex].remainingTime <= 1e-9){
                 jobs[chosenIndex].finished = 1;
                 jobs[chosenIndex].finishTime = now;
                 printf("DEBUG: Job finished => T=%d, jobId=%d at time=%.2f\n",
                        jobs[chosenIndex].taskIndex, jobs[chosenIndex].jobId, now);
             }
         }
     }
 }
 
 /*-----------------------------------------------------------
  * writeScheduleToFile
  *-----------------------------------------------------------*/
 static void writeScheduleToFile(const char* schedFile)
 {
     FILE* fp = fopen(schedFile, "w");
     if(!fp){
         printf("ERROR: Cannot open %s for writing.\n", schedFile);
         return;
     }
     fprintf(fp, "EDF-VD Schedule from 0 to each event:\n");
     for(int i=0; i<g_numSlices; i++){
         int tid   = slices[i].taskIndex;
         int jobid = slices[i].jobId;
         fprintf(fp, "[%6.2f -> %6.2f]: Task=%s Job=%d\n",
                 slices[i].start, slices[i].end,
                 tasks[tid].name, jobid);
     }
     fclose(fp);
 }
 
 /*-----------------------------------------------------------
  * analyzeSchedule
  *-----------------------------------------------------------*/
 static void analyzeSchedule(const char* analysisFile)
 {
     FILE* fp = fopen(analysisFile, "w");
     if(!fp){
         printf("ERROR: Cannot open %s for writing.\n", analysisFile);
         return;
     }
 
     int preemptions = 0;
     for(int i=1; i<g_numSlices; i++){
         /* Each time we switch from one job to another => preemption */
         if(slices[i].taskIndex != slices[i-1].taskIndex ||
            slices[i].jobId     != slices[i-1].jobId)
         {
             preemptions++;
         }
     }
 
     double totalWait = 0.0;
     double totalResp = 0.0;
     int finishedJobs = 0;
 
     for(int i=0; i<g_numJobs; i++){
         if(jobs[i].finished){
             double wait = jobs[i].startTime - jobs[i].arrivalTime;
             double resp = jobs[i].finishTime - jobs[i].arrivalTime;
             totalWait += wait;
             totalResp += resp;
             finishedJobs++;
         }
     }
 
     double avgWait = 0.0;
     double avgResp = 0.0;
     if(finishedJobs > 0) {
         avgWait = totalWait / finishedJobs;
         avgResp = totalResp / finishedJobs;
     }
 
     fprintf(fp, "EDF-VD Schedule Analysis\n");
     fprintf(fp, "========================\n");
     fprintf(fp, "Number of tasks : %d\n", g_numTasks);
     fprintf(fp, "Number of jobs  : %d\n", g_numJobs);
     fprintf(fp, "Preemptions     : %d\n", preemptions);
     fprintf(fp, "Avg Wait        : %.2f\n", avgWait);
     fprintf(fp, "Avg Response    : %.2f\n", avgResp);
 
     fclose(fp);
 }
 