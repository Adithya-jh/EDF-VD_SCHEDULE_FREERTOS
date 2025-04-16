/**
 * File: sim_offline_edfvd.c
 * A condensed version of the offline EDF-VD example. 
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <math.h>
 #include <string.h>
 #include "FreeRTOS.h"  // If you want, but not strictly needed for offline code
 #include "sim_offline_edfvd.h" // (optional header)
 
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
     int taskIndex;
     int jobId;
     double arrivalTime;
     double absoluteDeadline;
     double virtualDeadline;
     double wcet;            
     double actualExecTime;  
     double remainingTime;   
     double startTime;
     double finishTime;
     int finished;
 } Job_t;
 
 /* We'll store in file-scope arrays. */
 static TaskInfo_t tasks[MAX_TASKS];
 static Job_t      jobs[MAX_JOBS];
 
 static int g_numTasks = 0;
 static int g_numJobs  = 0;
//  static int g_numSlices = 0;
 
 /* local function prototypes */
 static void parseTaskFile(const char* filename);
 static void computeHyperPeriodAndJobCounts(double* hyperPeriod);
 static void computeEDFVDParameters(void);
 static void buildJobsArray(double hyperPeriod, const char* execTimesFile);
 static void scheduleEDFVD(double hyperPeriod);
 static void writeScheduleToFile(const char* schedFile);
 static void analyzeSchedule(const char* analysisFile);
 
 /* gcd, lcm for hyperperiod */
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
 
 /* A static array to store scheduling slices (time intervals). */
 typedef struct {
     double start;
     double end;
     int taskIndex;
     int jobId;
 } Slice_t;
 
 static Slice_t slices[10000];
 static int g_numSlices = 0;
 
 /* --------------------------------------------------------------
  * This function is called from the FreeRTOS simulation task.
  * --------------------------------------------------------------*/
 void vRunOfflineEDFVD(void)
 {
     const char* taskFile      = "tasks.txt";
     const char* execTimesFile = "exec_times.txt";
     const char* scheduleOut   = "schedule_output.txt";
     const char* analysisOut   = "schedule_analysis.txt";
 
     printf("vRunOfflineEDFVD: Reading tasks from %s...\n", taskFile);
     parseTaskFile(taskFile);
     printf("Parsed %d tasks.\n", g_numTasks);
 
     double hyperPeriod = 0;
     computeHyperPeriodAndJobCounts(&hyperPeriod);
     printf("Hyperperiod = %.2f\n", hyperPeriod);
 
     computeEDFVDParameters();
     buildJobsArray(hyperPeriod, execTimesFile);
 
     scheduleEDFVD(hyperPeriod);
     writeScheduleToFile(scheduleOut);
     analyzeSchedule(analysisOut);
 
     printf("Offline EDF-VD: done. Results in %s and %s.\n", scheduleOut, analysisOut);
 }
 
 /* --------------------------------------------------------------
  * 1) parseTaskFile
  * --------------------------------------------------------------*/
 static void parseTaskFile(const char* filename)
 {
     FILE* fp = fopen(filename, "r");
     if(!fp){
         printf("Cannot open %s.\n", filename);
         return;
     } else {
        printf("Successfully opened %s.\n", filename);
     }
     fscanf(fp, "%d", &g_numTasks);
     printf("DEBUG: g_numTasks = %d\n", g_numTasks);
     for(int i=0; i<g_numTasks; i++){
         char c;
         fscanf(fp, "%s", tasks[i].name);
         fscanf(fp, "%lf", &tasks[i].phase);
         fscanf(fp, "%lf", &tasks[i].period);
         fscanf(fp, "%lf", &tasks[i].wcet);
         fscanf(fp, "%lf", &tasks[i].deadline);
         fscanf(fp, " %c", &c);
         tasks[i].critLevel = (c=='H'||c=='h') ? CRIT_HIGH : CRIT_LOW;
         tasks[i].virtualDeadline = tasks[i].deadline;
         tasks[i].jobCount = 0;
     }
     fclose(fp);
 }
 
 /* --------------------------------------------------------------
  * 2) computeHyperPeriodAndJobCounts
  * --------------------------------------------------------------*/
 static void computeHyperPeriodAndJobCounts(double* hyperPeriod)
 {
     long long arr[50];
     for(int i=0; i<g_numTasks; i++){
         long long p = (long long) tasks[i].period;
         arr[i] = p;
     }
     long long l = 1;
     for(int i=0; i<g_numTasks; i++){
         l = lcmLL(l, arr[i]);
     }
     *hyperPeriod = (double) l;
 
     for(int i=0; i<g_numTasks; i++){
         double HP = *hyperPeriod;
         double ph = tasks[i].phase;
         double pd = tasks[i].period;
         if(ph > HP){
             tasks[i].jobCount = 0;
         } else {
             /* # of jobs in [0..HP) */
             int count = (int) floor((HP - ph) / pd);
             tasks[i].jobCount = count;
         }
     }
 }
 
 /* --------------------------------------------------------------
  * 3) computeEDFVDParameters
  * --------------------------------------------------------------*/
 static void computeEDFVDParameters(void)
 {
     double U_H = 0.0, U_L = 0.0;
     for(int i=0; i<g_numTasks; i++){
         double util = tasks[i].wcet / tasks[i].period;
         if(tasks[i].critLevel == CRIT_HIGH) U_H += util;
         else U_L += util;
     }
     double x = 1.0;
     if(U_L < 1.0){
         x = U_H / (1.0 - U_L);
         if(x > 1.0) x = 1.0;
     }
 
     for(int i=0; i<g_numTasks; i++){
         if(tasks[i].critLevel == CRIT_HIGH){
             tasks[i].virtualDeadline = tasks[i].deadline * x;
         }
     }
 }
 
 /* --------------------------------------------------------------
  * 4) buildJobsArray
  * --------------------------------------------------------------*/
 static void buildJobsArray(double hyperPeriod, const char* execTimesFile)
 {
     /* We'll read the actual execution times from 'execTimesFile'.
      * Each line has jobCount times for that task.
      */
     FILE* fp = fopen(execTimesFile, "r");
     if(!fp){
         printf("Cannot open exec times file.\n");
         return;
     }
 
     g_numJobs = 0;
     for(int t=0; t<g_numTasks; t++){
         int count = tasks[t].jobCount;
         double* actualETs = (double*)malloc(sizeof(double)*count);
         for(int j=0; j<count; j++){
             fscanf(fp, "%lf", &actualETs[j]);
         }
 
         for(int j=0; j<count; j++){
             double arrival = tasks[t].phase + j*tasks[t].period;
             if(arrival >= hyperPeriod) break;
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
             jobs[g_numJobs].startTime  = -1;
             jobs[g_numJobs].finishTime = -1;
 
             g_numJobs++;
         }
         free(actualETs);
     }
     fclose(fp);
 }
 
 /* --------------------------------------------------------------
  * 5) scheduleEDFVD
  * --------------------------------------------------------------*/
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
 
     while(now < hyperPeriod)
     {
         /* 1) find active jobs: arrivalTime <= now < finish, not finished */
         printf("DEBUG: Entering loop. now=%.2f\n", now);
         int activeCount = 0;
         printf("DEBUG: now=%.2f, activeCount=%d\n", now, activeCount);
         int activeIndices[2000];
         for(int i=0; i<g_numJobs; i++){
             if(!jobs[i].finished && 
                jobs[i].arrivalTime <= now &&
                jobs[i].remainingTime > 0.0)
             {
                 activeIndices[activeCount++] = i;
             }
         }
 
         if(activeCount == 0){
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
                 break;
             }
         }
 
         /* 2) among active, pick earliest VDL */
         if(activeCount > 1){
             Job_t tmp[2000];
             for(int i=0; i<activeCount; i++){
                 tmp[i] = jobs[ activeIndices[i] ];
             }
             qsort(tmp, activeCount, sizeof(Job_t), compareVDL);
             int chosen = -1;
             for(int i=0; i<activeCount; i++){
                 if( (jobs[activeIndices[i]].virtualDeadline == tmp[0].virtualDeadline) &&
                     (jobs[activeIndices[i]].taskIndex == tmp[0].taskIndex) )
                 {
                     chosen = activeIndices[i];
                     break;
                 }
             }
             if(chosen < 0) { /* error */ break; }
             /* next completion time or next arrival */
             double remain = jobs[chosen].remainingTime;
             double nextArrival = hyperPeriod;
             for(int i=0; i<g_numJobs; i++){
                 if(!jobs[i].finished && jobs[i].arrivalTime > now){
                     if(jobs[i].arrivalTime < nextArrival){
                         nextArrival = jobs[i].arrivalTime;
                     }
                 }
             }
             double finishIfUninterrupted = now + remain;
             double nextDecision = (nextArrival < finishIfUninterrupted) ? nextArrival : finishIfUninterrupted;
 
             /* record scheduling slice if new job or preemption */
             if(chosen != lastJobIndex){
                 g_numSlices++;
                 slices[g_numSlices-1].start     = now;
                 slices[g_numSlices-1].taskIndex = jobs[chosen].taskIndex;
                 slices[g_numSlices-1].jobId     = jobs[chosen].jobId;
                 lastJobIndex = chosen;
             }
             slices[g_numSlices-1].end = nextDecision;
 
             /* run chosen job from now to nextDecision */
             double delta = nextDecision - now;
             jobs[chosen].remainingTime -= delta;
             if(jobs[chosen].startTime < 0) jobs[chosen].startTime = now;
 
             now = nextDecision;
             if(jobs[chosen].remainingTime <= 1e-9){
                 jobs[chosen].finished = 1;
                 jobs[chosen].finishTime = now;
             }
         }
         else {
             int chosen = activeIndices[0];
             double remain = jobs[chosen].remainingTime;
             double nextArrival = hyperPeriod;
             for(int i=0; i<g_numJobs; i++){
                 if(!jobs[i].finished && jobs[i].arrivalTime > now){
                     if(jobs[i].arrivalTime < nextArrival) nextArrival = jobs[i].arrivalTime;
                 }
             }
             double finishIfUninterrupted = now + remain;
             double nextDecision = (nextArrival < finishIfUninterrupted) ? nextArrival : finishIfUninterrupted;
 
             if(chosen != lastJobIndex){
                 g_numSlices++;
                 slices[g_numSlices-1].start     = now;
                 slices[g_numSlices-1].taskIndex = jobs[chosen].taskIndex;
                 slices[g_numSlices-1].jobId     = jobs[chosen].jobId;
                 lastJobIndex = chosen;
             }
             slices[g_numSlices-1].end = nextDecision;
 
             double delta = nextDecision - now;
             jobs[chosen].remainingTime -= delta;
             if(jobs[chosen].startTime < 0) jobs[chosen].startTime = now;
 
             now = nextDecision;
             if(jobs[chosen].remainingTime <= 1e-9){
                 jobs[chosen].finished = 1;
                 jobs[chosen].finishTime = now;
             }
         }
     }
 }
 
 /* --------------------------------------------------------------
  * 6) writeScheduleToFile
  * --------------------------------------------------------------*/
 static void writeScheduleToFile(const char* schedFile)
 {
     FILE* fp = fopen(schedFile, "w");
     if(!fp){
         printf("Cannot open %s.\n", schedFile);
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
 
 /* --------------------------------------------------------------
  * 7) analyzeSchedule
  * --------------------------------------------------------------*/
 static void analyzeSchedule(const char* analysisFile)
 {
     FILE* fp = fopen(analysisFile, "w");
     if(!fp){
         printf("Cannot open %s.\n", analysisFile);
         return;
     }
 
     int preemptions = 0;
     for(int i=1; i<g_numSlices; i++){
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
 
     double avgWait = (finishedJobs>0)?(totalWait/finishedJobs):0.0;
     double avgResp = (finishedJobs>0)?(totalResp/finishedJobs):0.0;
 
     fprintf(fp, "EDF-VD Schedule Analysis\n");
     fprintf(fp, "========================\n");
     fprintf(fp, "Preemptions: %d\n", preemptions);
     fprintf(fp, "Avg Wait: %.2f\n", avgWait);
     fprintf(fp, "Avg Response: %.2f\n", avgResp);
 
     fclose(fp);
 }
 