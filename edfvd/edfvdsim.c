/****************************************************************************
 * EDF-VD Offline Scheduler Simulation
 *
 * Reads:
 *   1) A "tasks file" with N tasks:
 *      - # of tasks in the first line
 *      - Each subsequent line: taskName phase period wcet deadline critLevel
 *
 *   2) An "execution times file" that provides the actual exec times for each
 *      job of each task over the hyperperiod. The format could be something like:
 *        For each task, a line containing M actual execution times
 *        (M = number of job instances in the hyperperiod).
 *
 * Schedules from t=0 to t=hyperperiod using EDF-VD:
 *   - High-critical tasks have their deadlines scaled by x < 1
 *   - Low-critical tasks keep their deadlines
 *   - Only re-schedule at "decision points": arrivals or completions
 *
 * Outputs:
 *   1) schedule_output.txt   (the timeline of which job runs when)
 *   2) schedule_analysis.txt (preemptions, wait times, etc.)
 *
 * This is a reference skeleton. You may tailor naming, input format,
 * error checking, etc. to your exact assignment.
 ****************************************************************************/

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <math.h>
 
 /*-----------------------------------------------------------
  * Data Structures
  *-----------------------------------------------------------*/
 
 /* For a two-criticality system (High/Low). Adjust if needed. */
 typedef enum {
     CRIT_LOW = 0,
     CRIT_HIGH
 } CritLevel_t;
 
 /* Info about each *Task*, read from the tasks file. */
 typedef struct {
     char         name[32];
     double       phase;      /* initial offset */
     double       period;
     double       wcet;       /* worst-case execution time */
     double       deadline;
     CritLevel_t  critLevel;
     double       virtualDeadline; /* computed for EDF-VD */
     int          jobCount;   /* number of jobs in hyperperiod (filled later) */
 } TaskInfo_t;
 
 /* Info about each *Job* (an instance of a Task). */
 typedef struct {
     int     taskIndex;       /* which task does this job belong to? */
     int     jobId;           /* the job iteration index for that task (0,1,2...) */
     double  arrivalTime;     /* arrival = phase + jobId*period */
     double  absoluteDeadline;/* arrivalTime + realDeadline or *virtualDeadline? */
     double  virtualDeadline; /* used for sorting in EDF-VD */
     double  wcet;            /* worst-case. Not always used if we have actual exec times */
     double  actualExecTime;  /* from the second file, for this job iteration */
     double  remainingTime;   /* changes as the job executes */
     double  startTime;       /* when job first starts (for analysis) */
     double  finishTime;      /* when job completes (for analysis) */
     int     finished;        /* boolean: 1 if completed, 0 if not */
 } Job_t;
 
 /* For analyzing the schedule. We track each scheduling interval or "run slice." */
 typedef struct {
     double start;
     double end;
     int    taskIndex;
     int    jobId;
 } ScheduleSlice_t;
 
 /*-----------------------------------------------------------
  * Global / Config
  *-----------------------------------------------------------*/
 #define MAX_TASKS 50
 #define MAX_JOBS  5000  /* depends on how large the hyperperiod is */
 #define MAX_SLICES 10000
 
 static TaskInfo_t tasks[MAX_TASKS];
 static Job_t      jobs[MAX_JOBS];
 static ScheduleSlice_t slices[MAX_SLICES];
 
 static int g_numTasks = 0;
 static int g_numJobs  = 0;
 static int g_numSlices = 0;
 
 /*-----------------------------------------------------------
  * Function Prototypes
  *-----------------------------------------------------------*/
 void parseTaskFile(const char* filename);
 void computeHyperPeriodAndJobCounts(double* hyperPeriod);
 void computeEDFVDParameters();
 void parseExecTimesFile(const char* filename);
 void buildJobsArray(double hyperPeriod);
 void scheduleEDFVD(double hyperPeriod);
 void writeScheduleToFile(const char* filename);
 void analyzeSchedule(const char* filename);
 
 /* Helpers */
 static long long gcdLL(long long a, long long b);
 static long long lcmLL(long long a, long long b);
 
 /*-----------------------------------------------------------
  * main()
  *-----------------------------------------------------------*/
 int main(int argc, char* argv[])
 {
     /*
      * We assume no command-line arguments are used for the actual assignment
      * (your professor wants the file names inside the code or read from a config).
      * Adjust as needed.
      */
     const char* taskFile       = "tasks.txt";         /* Example name */
     const char* execTimesFile  = "exec_times.txt";    /* Example name */
     const char* scheduleOut    = "schedule_output.txt";
     const char* analysisOut    = "schedule_analysis.txt";
 
     printf("EDF-VD Offline Scheduler Simulation\n");
 
     /* 1. Parse tasks */
     parseTaskFile(taskFile);
     printf("Parsed %d tasks from %s.\n", g_numTasks, taskFile);
 
     /* 2. Compute hyperperiod (and the # of jobs each task can generate) */
     double hyperPeriod = 0;
     computeHyperPeriodAndJobCounts(&hyperPeriod);
     printf("HyperPeriod = %.2f\n", hyperPeriod);
 
     /* 3. Compute EDF-VD parameters (virtual deadlines for high-crit tasks) */
     computeEDFVDParameters();
 
     /* 4. Parse actual execution times for each job from second file */
     parseExecTimesFile(execTimesFile);
 
     /* 5. Build the jobs array (arrival times, deadlines, etc.) */
     buildJobsArray(hyperPeriod);
 
     /* 6. Run the EDF-VD scheduler from 0..hyperPeriod at decision points */
     scheduleEDFVD(hyperPeriod);
 
     /* 7. Write schedule to a file */
     writeScheduleToFile(scheduleOut);
     printf("Schedule written to %s.\n", scheduleOut);
 
     /* 8. Analyze schedule (preemptions, wait times, etc.) -> output file */
     analyzeSchedule(analysisOut);
     printf("Analysis written to %s.\n", analysisOut);
 
     printf("Done.\n");
     return 0;
 }
 
 /*-----------------------------------------------------------
  * 1) Parse the tasks file
  *-----------------------------------------------------------*/
 void parseTaskFile(const char* filename)
 {
     FILE* fp = fopen(filename, "r");
     if(!fp){
         fprintf(stderr, "Error opening tasks file: %s\n", filename);
         exit(1);
     }
 
     fscanf(fp, "%d", &g_numTasks);
     if(g_numTasks > MAX_TASKS){
         fprintf(stderr, "Too many tasks. Increase MAX_TASKS.\n");
         fclose(fp);
         exit(1);
     }
 
     for(int i=0; i<g_numTasks; i++){
         /* Example line format:
          * name phase period wcet deadline critLevel
          * e.g.: T1 0 10 3 10 H
          */
         char critChar;
         fscanf(fp, "%s", tasks[i].name);
         fscanf(fp, "%lf", &tasks[i].phase);
         fscanf(fp, "%lf", &tasks[i].period);
         fscanf(fp, "%lf", &tasks[i].wcet);
         fscanf(fp, "%lf", &tasks[i].deadline);
         fscanf(fp, " %c", &critChar);
 
         if(critChar == 'H' || critChar == 'h')
             tasks[i].critLevel = CRIT_HIGH;
         else
             tasks[i].critLevel = CRIT_LOW;
 
         tasks[i].virtualDeadline = tasks[i].deadline; /* default, will be scaled for high-crit if needed */
         tasks[i].jobCount = 0; /* filled later */
     }
 
     fclose(fp);
 }
 
 /*-----------------------------------------------------------
  * 2) Compute hyperperiod and job counts
  *-----------------------------------------------------------*/
 void computeHyperPeriodAndJobCounts(double* hyperPeriod)
 {
     /* For integer periods, typically we do an LCM. If your periods are
      * not necessarily integer, you might approximate or do something else.
      */
     long long intPeriods[MAX_TASKS];
     for(int i=0; i<g_numTasks; i++){
         double p = tasks[i].period;
         long long p_ll = (long long) p;
         if(fabs(p - p_ll) > 1e-9){
             fprintf(stderr, "[Warning] Period for task %s is not an integer. LCM might be inaccurate.\n",
                     tasks[i].name);
         }
         intPeriods[i] = p_ll;
     }
 
     long long l = 1;
     for(int i=0; i<g_numTasks; i++){
         l = lcmLL(l, intPeriods[i]);
     }
     *hyperPeriod = (double) l;
 
     /* compute how many jobs each task will have in [0..hyperPeriod) */
     for(int i=0; i<g_numTasks; i++){
         int count = (int)((*hyperPeriod - tasks[i].phase) / tasks[i].period);
         if(count < 0) count = 0;
         /* if tasks[i].phase == 0, then we get exactly floor(HP/period) instances */
         if(tasks[i].phase <= *hyperPeriod){
             count = (int) floor(((*hyperPeriod - tasks[i].phase)) / tasks[i].period);
             /* +1 if strictly we consider arrival at exactly HP doesn't produce a new job */
         }
         tasks[i].jobCount = count;
     }
 }
 
 /*-----------------------------------------------------------
  * 3) Compute EDF-VD parameters (virtual deadlines for high-crit tasks)
  *-----------------------------------------------------------*/
 void computeEDFVDParameters()
 {
     /* A typical 2-level approach for computing x:
      *   U_H = sum of (wcet / period) for high-critical tasks
      *   U_L = sum of (wcet / period) for low-critical tasks
      *   We want x so that U_H / x + U_L <= 1, and x <= 1
      */
     double U_H = 0.0, U_L = 0.0;
     for(int i=0; i<g_numTasks; i++){
         double util = tasks[i].wcet / tasks[i].period;
         if(tasks[i].critLevel == CRIT_HIGH) U_H += util;
         else U_L += util;
     }
 
     if(U_H > 1.0){
         fprintf(stderr, "[Warning] High-crit tasks alone exceed total utilization > 1.\n");
         /* Typically unschedulable, but let's keep going. */
     }
 
     /* Compute x if possible. The standard formula is x = U_H / (1.0 - U_L),
      * if U_L < 1. Otherwise, no feasible solution. We'll clamp x to 1 if it goes above 1.
      */
     double x = 1.0;
     if(U_L < 1.0){
         double denom = (1.0 - U_L);
         x = U_H / denom;
     }
     if(x > 1.0) x = 1.0;
 
     /* Scale the deadlines for high-critical tasks by x */
     for(int i=0; i<g_numTasks; i++){
         if(tasks[i].critLevel == CRIT_HIGH){
             tasks[i].virtualDeadline = tasks[i].deadline * x;
         }
     }
 
     /* For low-crit tasks, virtualDeadline = realDeadline. (already set by default) */
 }
 
 /*-----------------------------------------------------------
  * 4) Parse the Actual Execution Times file
  *-----------------------------------------------------------*/
 void parseExecTimesFile(const char* filename)
 {
     /* We assume each line in the file provides the actual execution times
      * for one Task, for each of its job instances (jobCount).
      *
      * Format example:
      * (For Task0) 2.5 2.0 2.9 ...  [jobCount0 times]
      * (For Task1) 1.0 1.2 1.5 ...
      * ...
      */
     FILE* fp = fopen(filename, "r");
     if(!fp){
         fprintf(stderr, "Error opening exec times file: %s\n", filename);
         exit(1);
     }
 
     for(int i=0; i<g_numTasks; i++){
         int count = tasks[i].jobCount;
         if(count > 0){
             /* We just read 'count' actual execution times for task i. 
              * We'll store them later in buildJobsArray, because we need
              * to map them to each job's structure.
              */
             // If you want, store them in a global 2D array: actualExecTimes[task i][job index].
             // For clarity, we'll allocate dynamically here:
         }
     }
 
     /* We'll do a simple approach: read line by line. 
      * Each line has 'count' doubles for the respective task i. 
      */
     for(int i=0; i<g_numTasks; i++){
         if(tasks[i].jobCount > 0){
             /* read one line containing jobCount actual times */
             // We can do it in a buffer or parse directly into a temporary array:
             // But for simplicity, we do it directly when building jobs array.
         }
     }
 
     fclose(fp);
 
     /* In a realistic approach, you'd store them in a data structure (like a 2D array)
      * so that buildJobsArray can assign them. For demonstration, let's read them
      * in buildJobsArray (which also needs the file).
      *
      * We'll do the actual read in buildJobsArray to keep everything in sync.
      * So for now, do nothing or skip. 
      */
 }
 
 /*-----------------------------------------------------------
  * 5) Build the Jobs array
  *    For each task, for each instance in [0..hyperPeriod),
  *    create a Job object with arrivalTime, deadlines, etc.
  *    Then read the actual execution times for each job from the file again.
  *-----------------------------------------------------------*/
 void buildJobsArray(double hyperPeriod)
 {
     /* Let's open the exec_times file again, read line by line. 
      * Alternatively, we might do it once above, but let's show a clear approach.
      */
     const char* execTimesFile = "exec_times.txt"; // same as before
     FILE* fp = fopen(execTimesFile, "r");
     if(!fp){
         fprintf(stderr, "Error re-opening exec times file.\n");
         exit(1);
     }
 
     g_numJobs = 0;
     for(int tIndex=0; tIndex<g_numTasks; tIndex++){
         int count = tasks[tIndex].jobCount;
         /* read 'count' actual execution times from the next line */
         double* actualTimes = (double*) malloc(sizeof(double)*count);
         for(int j=0; j<count; j++){
             fscanf(fp, "%lf", &actualTimes[j]);
         }
 
         /* Create each job structure */
         for(int jobId = 0; jobId < count; jobId++){
             double arrival = tasks[tIndex].phase + jobId * tasks[tIndex].period;
             if(arrival >= hyperPeriod) break; // skip if it doesn't start before HP
             jobs[g_numJobs].taskIndex       = tIndex;
             jobs[g_numJobs].jobId           = jobId;
             jobs[g_numJobs].arrivalTime     = arrival;
             jobs[g_numJobs].wcet            = tasks[tIndex].wcet;
             jobs[g_numJobs].actualExecTime  = actualTimes[jobId];
             jobs[g_numJobs].remainingTime   = actualTimes[jobId];
             jobs[g_numJobs].finished        = 0;
 
             /* Real absolute deadline = arrival + tasks[tIndex].deadline */
             double realDL = arrival + tasks[tIndex].deadline;
             /* Virtual deadline if high crit: arrival + tasks[tIndex].virtualDeadline */
             double vDL     = arrival + tasks[tIndex].virtualDeadline;
 
             jobs[g_numJobs].absoluteDeadline = realDL;
             jobs[g_numJobs].virtualDeadline  = vDL;
 
             /* For analysis: start/finish times not known yet */
             jobs[g_numJobs].startTime  = -1.0;
             jobs[g_numJobs].finishTime = -1.0;
 
             g_numJobs++;
             if(g_numJobs >= MAX_JOBS){
                 fprintf(stderr,"Too many jobs. Increase MAX_JOBS.\n");
                 free(actualTimes);
                 fclose(fp);
                 exit(1);
             }
         }
         free(actualTimes);
     }
 
     fclose(fp);
 }
 
 /*-----------------------------------------------------------
  * 6) EDF-VD scheduling from 0..hyperPeriod at decision points
  *-----------------------------------------------------------*/
 static int compareVDL(const void* a, const void* b)
 {
     /* Sort by earliest virtual deadline */
     const Job_t* ja = (const Job_t*)a;
     const Job_t* jb = (const Job_t*)b;
     if(ja->virtualDeadline < jb->virtualDeadline) return -1;
     else if(ja->virtualDeadline > jb->virtualDeadline) return 1;
     else return 0;
 }
 
 void scheduleEDFVD(double hyperPeriod)
 {
     double currentTime = 0.0;
     g_numSlices = 0;
     int lastRunningJobIndex = -1;
 
     /* We’ll store events: arrivals and completions. We do
      * a nextEventTime approach. The “decision points” are:
      *   - The next arrival among un-started jobs
      *   - The completion time of the currently running job
      *
      * We run until currentTime >= hyperPeriod or no active jobs left.
      */
     while(currentTime < hyperPeriod)
     {
         /* 1) Collect active jobs (arrived, not finished, arrival <= currentTime, and remainingTime>0) */
         int activeCount = 0;
         int activeIndices[MAX_JOBS];
         for(int i=0; i<g_numJobs; i++){
             if(!jobs[i].finished &&
                jobs[i].arrivalTime <= currentTime &&
                jobs[i].remainingTime > 0.0)
             {
                 activeIndices[activeCount++] = i;
             }
         }
 
         if(activeCount == 0){
             /* No active jobs at this moment. Jump to the next arrival time if any. */
             double nextArrival = hyperPeriod;
             for(int j=0; j<g_numJobs; j++){
                 if(!jobs[j].finished && jobs[j].arrivalTime > currentTime){
                     if(jobs[j].arrivalTime < nextArrival){
                         nextArrival = jobs[j].arrivalTime;
                     }
                 }
             }
 
             if(nextArrival > currentTime && nextArrival < hyperPeriod){
                 currentTime = nextArrival;  /* jump forward */
                 continue;
             } else {
                 /* no more arrivals, we can break out */
                 break;
             }
         }
 
         /* 2) Among active jobs, pick the one with earliest virtual deadline (EDF-VD) */
         if(activeCount > 1){
             /* We can sort them by virtualDeadline or just find min. */
             /* For clarity, let's build a temp array. */
             Job_t tmpArr[MAX_JOBS];
             for(int j=0; j<activeCount; j++){
                 tmpArr[j] = jobs[ activeIndices[j] ];
             }
             qsort(tmpArr, activeCount, sizeof(Job_t), compareVDL);
             /* The first in tmpArr is earliest VDL */
             int chosenId = tmpArr[0].taskIndex; 
             double chosenVDL = tmpArr[0].virtualDeadline;
             int chosenIndex = -1;
             for(int j=0; j<activeCount; j++){
                 if((jobs[activeIndices[j]].taskIndex == chosenId) &&
                    (jobs[activeIndices[j]].virtualDeadline == chosenVDL))
                 {
                     chosenIndex = activeIndices[j];
                     break;
                 }
             }
             /* chosenIndex is the job we run. */
             if(chosenIndex == -1){
                 fprintf(stderr, "Error selecting job.\n");
                 return;
             }
             /* 3) Decide how long we can run before next arrival or finishing this job. */
             double remain = jobs[chosenIndex].remainingTime;
             double nextArrival = hyperPeriod;
             for(int k=0; k<g_numJobs; k++){
                 if(!jobs[k].finished && jobs[k].arrivalTime > currentTime){
                     if(jobs[k].arrivalTime < nextArrival){
                         nextArrival = jobs[k].arrivalTime;
                     }
                 }
             }
 
             double nextCompletion = currentTime + remain; // if we run this job to finish
             double nextDecision = (nextArrival < nextCompletion) ? nextArrival : nextCompletion;
 
             /* If we are switching jobs, that's a preemption (unless it's the same job). */
             if(chosenIndex != lastRunningJobIndex){
                 /* record a new slice in schedule. */
                 g_numSlices++;
                 if(g_numSlices >= MAX_SLICES){
                     fprintf(stderr, "Too many schedule slices. Increase MAX_SLICES.\n");
                     return;
                 }
                 slices[g_numSlices-1].start     = currentTime;
                 slices[g_numSlices-1].taskIndex = jobs[chosenIndex].taskIndex;
                 slices[g_numSlices-1].jobId     = jobs[chosenIndex].jobId;
                 /* end time is unknown yet, will set after we know nextDecision */
                 lastRunningJobIndex = chosenIndex;
 
                 /* If job not started before, record its startTime. */
                 if(jobs[chosenIndex].startTime < 0){
                     jobs[chosenIndex].startTime = currentTime;
                 }
             }
 
             /* run chosen job from currentTime to nextDecision */
             double delta = nextDecision - currentTime;
             jobs[chosenIndex].remainingTime -= delta;
             currentTime = nextDecision;
 
             /* finish slice if we reached completion or next arrival. */
             slices[g_numSlices-1].end = currentTime;
 
             /* if the job is now finished, mark it, record finishTime */
             if(jobs[chosenIndex].remainingTime <= 1e-9){
                 jobs[chosenIndex].finished = 1;
                 jobs[chosenIndex].finishTime = currentTime;
             }
         }
         else {
             /* Only one active job, simpler. */
             int chosenIndex = activeIndices[0];
             double remain = jobs[chosenIndex].remainingTime;
 
             double nextArrival = hyperPeriod;
             for(int k=0; k<g_numJobs; k++){
                 if(!jobs[k].finished && jobs[k].arrivalTime > currentTime){
                     if(jobs[k].arrivalTime < nextArrival){
                         nextArrival = jobs[k].arrivalTime;
                     }
                 }
             }
             double nextCompletion = currentTime + remain; 
             double nextDecision = (nextArrival < nextCompletion)? nextArrival : nextCompletion;
 
             if(chosenIndex != lastRunningJobIndex){
                 /* new slice */
                 g_numSlices++;
                 if(g_numSlices >= MAX_SLICES){
                     fprintf(stderr, "Too many schedule slices.\n");
                     return;
                 }
                 slices[g_numSlices-1].start     = currentTime;
                 slices[g_numSlices-1].taskIndex = jobs[chosenIndex].taskIndex;
                 slices[g_numSlices-1].jobId     = jobs[chosenIndex].jobId;
                 lastRunningJobIndex             = chosenIndex;
 
                 if(jobs[chosenIndex].startTime < 0){
                     jobs[chosenIndex].startTime = currentTime;
                 }
             }
 
             double delta = nextDecision - currentTime;
             jobs[chosenIndex].remainingTime -= delta;
             currentTime = nextDecision;
             slices[g_numSlices-1].end = currentTime;
 
             if(jobs[chosenIndex].remainingTime <= 1e-9){
                 jobs[chosenIndex].finished = 1;
                 jobs[chosenIndex].finishTime = currentTime;
             }
         }
 
         /* loop continues until we surpass hyperPeriod or no active jobs remain. */
         /* We'll break if currentTime >= hyperPeriod in next iteration. */
     }
 }
 
 /*-----------------------------------------------------------
  * 7) Write schedule to a file
  *-----------------------------------------------------------*/
 void writeScheduleToFile(const char* filename)
 {
     FILE* fp = fopen(filename, "w");
     if(!fp){
         fprintf(stderr, "Cannot open %s for writing.\n", filename);
         return;
     }
 
     fprintf(fp, "EDF-VD Schedule from 0 to each event:\n");
     for(int i=0; i<g_numSlices; i++){
         int tid   = slices[i].taskIndex;
         int jobId = slices[i].jobId;
         double st = slices[i].start;
         double en = slices[i].end;
         fprintf(fp, "[%6.2f -> %6.2f]: Task=%s Job=%d\n", st, en, tasks[tid].name, jobId);
     }
 
     fclose(fp);
 }
 
 /*-----------------------------------------------------------
  * 8) Analyze schedule
  *-----------------------------------------------------------*/
 void analyzeSchedule(const char* filename)
 {
     /* We'll compute:
      * - # of preemptions
      * - average waiting time = (startTime - arrivalTime) for each job
      * - average response time = (finishTime - arrivalTime)
      * - maybe jitter, etc.
      */
     int preemptions = 0;
     double totalWait      = 0.0;
     double totalResponse  = 0.0;
     int finishedJobs      = 0;
 
     /* Count preemptions by checking slices array: each time we change (taskIndex,jobId). */
     for(int i=1; i<g_numSlices; i++){
         if(slices[i].taskIndex != slices[i-1].taskIndex ||
            slices[i].jobId     != slices[i-1].jobId)
         {
             preemptions++;
         }
     }
 
     /* waiting time & response time per job */
     for(int i=0; i<g_numJobs; i++){
         if(jobs[i].finished){
             double wait    = jobs[i].startTime  - jobs[i].arrivalTime;
             double resp    = jobs[i].finishTime - jobs[i].arrivalTime;
             totalWait     += wait;
             totalResponse += resp;
             finishedJobs++;
         }
     }
 
     double avgWait      = (finishedJobs > 0) ? (totalWait      / finishedJobs) : 0.0;
     double avgResponse  = (finishedJobs > 0) ? (totalResponse  / finishedJobs) : 0.0;
 
     /* Write results to file */
     FILE* fp = fopen(filename, "w");
     if(!fp){
         fprintf(stderr, "Cannot open %s for writing.\n", filename);
         return;
     }
 
     fprintf(fp, "EDF-VD Schedule Analysis\n");
     fprintf(fp, "========================\n");
     fprintf(fp, "Number of Preemptions: %d\n", preemptions);
     fprintf(fp, "Average Waiting Time:  %.2f\n", avgWait);
     fprintf(fp, "Average Response Time: %.2f\n", avgResponse);
 
     /* You could also compute jitter, turnaround times, etc. if needed. */
 
     fclose(fp);
 }
 
 /*-----------------------------------------------------------
  * Helpers: GCD/LCM
  *-----------------------------------------------------------*/
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
 