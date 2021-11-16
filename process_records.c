#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "report_record_formats.h"
#include "queue_ids.h"
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <semaphore.h>

pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
sem_t sem;

//  struct to save the report request fields
typedef struct countStructure {
    int totalRecordCount;
    int threadCount;
    int threadID;
    int recordCount;
    char searchString[SEARCH_STRING_FIELD_LENGTH];
} countStruct;

static void signalHandler(int sig) {
    pthread_cond_signal(&cv);
}

void *printStatusReport(void *args) {

    countStruct * reportCounts = (countStruct *) args;
    int value;

    sem_getvalue(&sem, &value);

    do {
        pthread_mutex_lock(&mutex);
        pthread_cond_wait(&cv, &mutex);

        printf("***Report***\n");
        printf("%i records read for %i reports\n", reportCounts[0].totalRecordCount, reportCounts[0].threadCount);
        int i;
        for(i = 0; i < reportCounts[0].threadCount; i++) {
            printf("Records sent for report index %i: %i\n", reportCounts[i].threadID, reportCounts[i].recordCount);
        }
        pthread_mutex_unlock(&mutex);
        sem_getvalue(&sem, &value);

    
    } while(value == 1);

    free(reportCounts);



}

void finalStatusReportPrint(countStruct * reportCounts) {


    printf("***Report***\n");
    printf("%i records read for %i reports\n", reportCounts[0].totalRecordCount, reportCounts[0].threadCount);
    int i;
    for(i = 0; i < reportCounts[0].threadCount; i++) {
        printf("Records sent for report index %i: %i\n", reportCounts[i].threadID, reportCounts[i].recordCount);
    }
}


int main(int argc, char**argv) {
    int msqid;
    int ret;
    int msgflg = IPC_CREAT | 0666;
    key_t key;
    report_request_buf rbuf;
    report_record_buf sbuf;
    size_t buf_length;
    countStruct * reportCountStruct;

    sem_init(&sem, 0, 1);

    

    int threadCount = 0;
    int recordsRead = 0;
    int threadIndex;
    int currentThread;
  
    //  retrieve report requests, save into countstruct
    do {    
        key = ftok(FILE_IN_HOME_DIR,QUEUE_NUMBER);
        if (key == 0xffffffff) {
            fprintf(stderr,"Key cannot be 0xffffffff..fix queue_ids.h to link to existing file\n");
            return 1;
        }
        //  gets message 
        if ((msqid = msgget(key, msgflg)) < 0) {
            int errnum = errno;
            fprintf(stderr, "Value of errno: %d\n", errno);
            perror("(msgget)");
            fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
        }
        else
            fprintf(stderr, "msgget: msgget succeeded: msgqid = %d\n", msqid);


        do {
            ret = msgrcv(msqid, &rbuf, sizeof(rbuf), 1, 0);    //   receive type 1 message

            int errnum = errno;
            if (ret < 0 && errno !=EINTR){
                fprintf(stderr, "Value of errno: %d\n", errno);
                perror("Error printed by perror");
                fprintf(stderr, "Error receiving msg: %s\n", strerror( errnum ));
            }
        } while ((ret < 0 ) && (errno == 4));


        if(threadCount == 0) {
            threadCount = rbuf.report_count;
            threadIndex = threadCount;
            reportCountStruct = malloc(threadCount * sizeof(countStruct));
            int i;
            for(i = 0; i < threadCount; i++) {
                reportCountStruct[i].totalRecordCount = 0;
                reportCountStruct[i].threadCount = 0;
                reportCountStruct[i].recordCount = 0;
                reportCountStruct[i].threadID = 0;
                strcpy(reportCountStruct[i].searchString, "");
            }
        } else {    // handle possible error where threadCount isnt -1??
            fprintf(stderr, "Error initializing count structure\n");
        }



        currentThread = rbuf.report_idx;
        
        //  0 index of array holds the values for the report creator
        reportCountStruct[0].threadCount = threadCount;
        //  used the current thread as an index so the threads were in order to simplify the report
        reportCountStruct[currentThread - 1].threadID = rbuf.report_idx;
        strcpy(reportCountStruct[currentThread - 1].searchString, rbuf.search_string);

        threadIndex--;


    } while(threadIndex > 0);


    //  thread creation also refer to comment at the end of main()
    pthread_t statusReportThread;
    pthread_create(&statusReportThread, NULL, printStatusReport, (void *)reportCountStruct);
    signal(SIGINT, signalHandler);
    


    //  retrieve records

    sbuf.mtype = 2; //  used to exist in do while
    char record[RECORD_FIELD_LENGTH]; // was str

    do {
        fgets(record, RECORD_FIELD_LENGTH, stdin);
        
        int i;
        for(i = 0; i < threadCount; i++) {
            if(strstr(record, reportCountStruct[i].searchString) != 0) {

                key = ftok(FILE_IN_HOME_DIR, reportCountStruct[i].threadID);
                if (key == 0xffffffff) {
                    fprintf(stderr,"Key cannot be 0xffffffff..fix queue_ids.h to link to existing file\n");
                    return 1;
                }

                if ((msqid = msgget(key, msgflg)) < 0) {
                    int errnum = errno;
                    fprintf(stderr, "Value of errno: %d\n", errno);
                    perror("(msgget)");
                    fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
                }
                else
                    fprintf(stderr, "msgget: msgget succeeded: msgqid = %d\n", msqid);

                strcpy(sbuf.record, record);
                buf_length = strlen(sbuf.record) + sizeof(int) + 1;
               

                //  lock mutex while writing value
                pthread_mutex_lock(&mutex);
                reportCountStruct[i].recordCount++;
                pthread_mutex_unlock(&mutex);

                if((msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT)) < 0) {
                    int errnum = errno;
                    fprintf(stderr,"%d, %ld, %s %d\n", msqid, sbuf.mtype, sbuf.record, (int)buf_length);
                    perror("(msgsnd)");
                    fprintf(stderr, "Error sending msg: %s\n", strerror( errnum ));
                    exit(1);
                }
                else
                    fprintf(stderr,"msgsnd-report_record: record\"%s\" Sent (%d bytes)\n", sbuf.record,(int)buf_length);


                
            }
        }

        if(strcmp(record, "\n") != 0) {
            pthread_mutex_lock(&mutex);
            reportCountStruct[0].totalRecordCount++;    //  position 0 of the struct keeps track of the record count so thread can view it
            recordsRead++;
            pthread_mutex_unlock(&mutex);
            if(recordsRead > 0 && (recordsRead % 10 == 0)) {    //  delay with a loop because it still allows the sigint to run, even if it is inefficient
                time_t startTime = time(NULL);
                while(difftime(time(NULL), startTime) < 5);
            }
        }

    } while(strcmp(record, "\n") != 0);

    


    int i;
    for(i = 0; i < threadCount; i++) {

        key = ftok(FILE_IN_HOME_DIR, reportCountStruct[i].threadID);
        if (key == 0xffffffff) {
            fprintf(stderr,"Key cannot be 0xffffffff..fix queue_ids.h to link to existing file\n");
            return 1;
        }

        if ((msqid = msgget(key, msgflg)) < 0) {
            int errnum = errno;
            fprintf(stderr, "Value of errno: %d\n", errno);
            perror("(msgget)");
            fprintf(stderr, "Error msgget: %s\n", strerror( errnum ));
        }
        else
            fprintf(stderr, "msgget: msgget succeeded: msgqid = %d\n", msqid);


        sbuf.mtype = 2;
        sbuf.record[0]=0;
        // strcpy(sbuf.record, "");
        buf_length = strlen(sbuf.record) + sizeof(int)+1;//struct size without
        // Send a message.
        if((msgsnd(msqid, &sbuf, buf_length, IPC_NOWAIT)) < 0) {
            int errnum = errno;
            fprintf(stderr,"%d, %ld, %s, %d\n", msqid, sbuf.mtype, sbuf.record, (int)buf_length);
            perror("(msgsnd)");
            fprintf(stderr, "Error sending msg: %s\n", strerror( errnum ));
            exit(1);
        }
        else
            fprintf(stderr,"msgsnd-report_record: record\"%s\" Sent (%d bytes)\n", sbuf.record,(int)buf_length);

    }

    finalStatusReportPrint(reportCountStruct);  //  just using this to manually print after not being able to figure it out

    
    sem_wait(&sem);
    
    pthread_cond_signal(&cv);   // condition signal
    // signalHandler(0);

    // pthread_join(statusReportThread, NULL);      //  this program would not run correctly 100% of the time, with pthread_join implemented
                                                    //  the thread waits for the condition but the condition signal
                                                    //  here didnt work but the signal in sigint did
                                                    

    free(reportCountStruct);

 

    exit(0);
}
