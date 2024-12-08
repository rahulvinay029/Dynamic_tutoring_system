/* Including all the necesary header files */

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

long long start_time;
/* Defining necessary semophores */
sem_t student_coordinator_sem, coordinator_to_tutor_sem;
/* variable to keep track of number of students received help */
int tutored_students;
/* variable to keep track of total number of requests */
int total_requests;
/* Defining a data sturucture for Students and including necessary fields*/
typedef struct {
    int id;
    int help_taken;
    int status;

} Student;

/*-1 -> Programming
 -2 -> Waiting for tutor */

/* Defining a data structure for Chairs and including all the necessary fields */
typedef struct{
    int student_in_chair;
    long long student_in_chairs_arrived_at;
}Chair;

/* Defining a Node Data structure */
typedef struct Node{
    Student data;
    struct Node* next;
}Node;

/* Defining a Queue data structure */
typedef struct Queue{
    struct Node* front;
    struct Node* rear;
}Queue;

/* Creating a waiting queue to keep track of waiting students*/
/* This is a shared data structure between student thread and coordinator thread */
Queue waiting_queue;
int num_students, num_tutors, num_chairs, help_count, available_chairs;
Student* students;

/* Creating a priority queue in which students are arranged by coordinator based on their priority */
Queue** priority_queues;
/* Declaring all the necessary locks */
pthread_mutex_t students_chairs_lock;
pthread_mutex_t queue_mutex;
pthread_mutex_t tutors_shared_variables_lock;
pthread_mutex_t priority_queues_lock;
pthread_mutex_t stdquelock;
pthread_mutex_t studentexitlock;
pthread_mutex_t shared_lock;

int currentNoOfTutoring, totalTutored;

/* Method to enque students into the waiting queue */
void enqueue(Queue* q, Student data) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }
 
    newNode->data = data;
    newNode->next = NULL;
 
    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = q->rear->next;
    }
}

/* Method to dequeue students from the waiting queue */
Node* dequeue(Queue* q) {
    if (q->front == NULL) {
       
        fflush(stdout);
        return NULL; 
    }
 
    Node* temp = q->front;
    q->front = q->front->next;
 
    if (q->front == NULL) {
        q->rear = NULL;
    }
 
    return temp;
}
/* Creating a First In First Out Queue */
void initFIFOQueue(Queue* q) {
    q->front = q->rear = NULL;
}

/* Method to keep track of number of students in the waiting queue */
int waiting_queue_length() {
    int length = 0;
    Node* current = waiting_queue.front;
 
    while (current != NULL) {
        length++;
        current = current->next;
    }
 
    return length;
}

/* Method to enqueue students into the priority queue based on their priority */
void enqueueToPriorityQueue(Student student, int helpCout) {
    Node* newNode = (Node*)malloc(sizeof(Node));
    if (newNode == NULL) {
        perror("Memory allocation error");
        exit(EXIT_FAILURE);
    }

    newNode->data = student;
    newNode->next = NULL;

    Queue* q = priority_queues[helpCout] ;
        if (q == NULL) {
        /*Initialize the queue*/
        q = (Queue*)malloc(sizeof(Queue));
        if (q == NULL) {
            perror("Memory allocation error");
            exit(EXIT_FAILURE);
        }
        q->front = q->rear = NULL;
        priority_queues[helpCout] = q;
    }
     if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }

}

/* Method to dequeue student of highest priority from the priority queue */
Node* dequeue_highest_priority( ) {
    if (priority_queues == NULL || help_count <= 0) {
        /* Handle invalid input */
        return NULL;
    }

    /* Iterate over the rows of queues */
    for (int i = 0; i < help_count; i++) {
        /* Check if the current queue is not empty */
        if (priority_queues[i] != NULL && priority_queues[i]->front != NULL) {
            /* Retrieve the front node */
            Node* dequeuedNode = priority_queues[i]->front;

            /* Update the front pointer */
            priority_queues[i]->front = priority_queues[i]->front->next;

            /* If the queue is now empty, update the rear pointer as well*/
            if (priority_queues[i]->front == NULL) {
                priority_queues[i]->rear = NULL;
            }

            /* Update the current row */
            return dequeuedNode;
        }
    }

    
    return NULL;
}

/* Beginning of Tutor thread */
void* tutor_thread_func(void* arg) {
    int tutorId = (intptr_t)arg;
 
    while (1) {
        /* Check if all students are tutored*/
        if(tutored_students == num_students){
           
           /* terminate threads is all students exited */
            pthread_exit(NULL);
        }
        
        sem_wait(&coordinator_to_tutor_sem);
 
        
        
 
        /* Acquire the lock */
        pthread_mutex_lock(&priority_queues_lock);

        /* Find the student with the highest priority */

        Student currentStudent = dequeue_highest_priority()->data;
        
        /* Release the lock*/
        pthread_mutex_unlock(&priority_queues_lock);


        if(currentStudent.id == -1){
            
            continue;
        }
        else{
           
                        /* Acquire the lock */
                        pthread_mutex_lock(&students_chairs_lock);
                        available_chairs++;
                        students[currentStudent.id].status = tutorId; /* Change the staus of the student */
                        currentNoOfTutoring++; 
                        pthread_mutex_unlock(&students_chairs_lock); /* Release the lock */

                        sleep(0.0002);
                         /* Acquire the lock */
                        pthread_mutex_lock(&students_chairs_lock);
                        totalTutored++;
                        printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. Total sessions tutored = %d\n",
                        currentStudent.id, tutorId, currentNoOfTutoring,totalTutored);
                        currentNoOfTutoring--;
                        /* Release the lock */
                        pthread_mutex_unlock(&students_chairs_lock);
        }
    }
}

/* Beginning of coordinator thread */
void *coordinator_thread_func(){
    while(true){
      
         /* Check if all students are tutored*/
        if(tutored_students == num_students){
       
             int i;
             for (i = 0; i < num_tutors; i++)
             {
               /* notify all the tutor threads */
               sem_post(&coordinator_to_tutor_sem);
            }
            pthread_exit(NULL);
          }
        /* wait for student thread to notify about a waiting student */
        sem_wait(&student_coordinator_sem);
 
         while(1){
         /* Acquiring a shared lock between students_chairs_lock and priority_queues_lock to handle deadlock situation */   
         pthread_mutex_lock(&shared_lock);
         /* Acquire the lock */
         pthread_mutex_lock(&stdquelock);
         Node* studentNode = dequeue(&waiting_queue);
         /* Release the lock */
         pthread_mutex_unlock(&stdquelock);
         /* setting priority of each student to zero */
         int priority =0;
         if(studentNode != NULL){
            /* Acquire the studentchairs lock */
            pthread_mutex_lock(&students_chairs_lock);
            Student currentStudent = studentNode -> data ;
            /* Updating priority based on helps taken and time arrived */
            priority = help_count - currentStudent.help_taken ;
            /* Acquiring lock for the priority queue */
            pthread_mutex_lock(&priority_queues_lock);
            /* enqueuing student to the priority queue */
            enqueueToPriorityQueue(currentStudent, currentStudent.help_taken);
            /* Release the lock */
            pthread_mutex_unlock(&priority_queues_lock);
             /* Updating the student status */           
            students[currentStudent.id].status = -2;
        printf("C: Student %d with priority %d added to the queue. Waiting students now = %d. Total requests = %d\n",currentStudent.id, priority, num_chairs - available_chairs, total_requests);
        /* Release the lock */
        pthread_mutex_unlock(&students_chairs_lock);
        /* Notifying the coordinator of waiting students */
        sem_post(&coordinator_to_tutor_sem);
        pthread_mutex_unlock(&shared_lock);
        /* Release the shared lock */
         }
         else{
            pthread_mutex_unlock(&shared_lock);
            break;
         }          
         }

    }
}
/* Beginning of the student thread */
void *student_thread_func(void* x){
    /* getting the studentID*/
    int studentId = (intptr_t)x;
    /* keeping track of number of helps taken by that particular student */
    int helps_taken = 0;
    
    while (helps_taken < help_count) {
        /* Setting a random seed */
        float random_number = (float) rand()/ RAND_MAX;
        double sleep_time = random_number * 0.002;
        
        sleep(sleep_time); 
        
        /* Acquire the studentchairs lock */
        pthread_mutex_lock(&students_chairs_lock);

            /* Checking for available chairs */
            if (available_chairs > 0) {
                total_requests++;
                /* updating number of chiars after a student takes a seat */
                available_chairs--;
                /* Acquiring the lock and adding the student to the waiting queue based on his time of arrival */
                pthread_mutex_lock(&stdquelock);
                enqueue(&waiting_queue, students[studentId]);
                /* Releasing the lock */
                pthread_mutex_unlock(&stdquelock);

                printf("S: Student %d takes a seat. Empty chairs = %d.\n", studentId, available_chairs);
                /* Releasing the lock */
                pthread_mutex_unlock(&students_chairs_lock);
                /* Notifying the coordinator that a student has arrived */
                sem_post(&student_coordinator_sem);
                /* wait while the student is getting tutored */
                while(students[studentId].status < 0);
                printf("S: Student %d received help from Tutor %d\n", studentId, students[studentId].status);
                    
                pthread_mutex_lock(&students_chairs_lock);
                helps_taken++;
                /* update number of helps that student has received in the data structure */
                students[studentId].help_taken = helps_taken;
                /* update his status to programming */
                students[studentId].status = -1; 
      
    pthread_mutex_unlock(&students_chairs_lock);
    }
    /* continue if there are no available chairs at the moment */
    else{
        printf("S: Student %d found no empty chair. Will try again later.\n", studentId);
        pthread_mutex_unlock(&students_chairs_lock);
        sleep(0.002);
        flag=0;
        continue;
    }
    
    }
    /* Track number of student that no longer need tutor help */
    pthread_mutex_lock(&students_chairs_lock);
    tutored_students++;
    pthread_mutex_unlock(&students_chairs_lock);
    /* exit the thread if all the students are finished receiving help */
    if(num_students==tutored_students){
                         exit(0);
                        }
    /* notify the coordinator */
    sem_post(&student_coordinator_sem);
    pthread_exit(NULL);
    return NULL;
}

int main(int argc, char* argv[]){
    /* If more than five command line arguments are given throw an error */
    if (argc != 5){
        return EXIT_FAILURE;
    }
    /* Take all the command line inputs */
    num_students = atoi(argv[1]);
    num_tutors = atoi(argv[2]);
    num_chairs = atoi(argv[3]);
    help_count = atoi(argv[4]);
    available_chairs = num_chairs;

    

    srand((unsigned int)time(NULL));
    start_time = clock();
    /* Intializing all the necessary locks */
    pthread_mutex_init(&students_chairs_lock, NULL);
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&tutors_shared_variables_lock, NULL);
    pthread_mutex_init(&priority_queues_lock, NULL);
    pthread_mutex_init(&stdquelock, NULL);
    pthread_mutex_init(&studentexitlock, NULL);
    pthread_mutex_init(&shared_lock, NULL);

    /* intializing the sempophores */
    sem_init(&coordinator_to_tutor_sem, 0, 0);
    sem_init(&student_coordinator_sem, 0, 0);

    /* allocating memory to student */
    students = (Student*)malloc(num_students * sizeof(Student));
    total_requests=0;
    int i=0;
    /* intializing all the students */
    for(i=0; i< num_students; i++){
        students[i].help_taken = 0;
        students[i].status = -1; // status 0 -> programming
        students[i].id = i;
    }

/* allocating memory for the priority queues */
 priority_queues = (Queue**)malloc(help_count * sizeof(Queue*));

int k=0;
for (k = 0; k < help_count; k++) {    
    priority_queues[k] = (Queue*)malloc(num_students*sizeof(Queue));
    priority_queues[k]->front = priority_queues[k]->rear = NULL ;
}
/* initializing the student threads */
    pthread_t student_threads[num_students], tutor_threads[num_tutors];
pthread_t coordinator_thread;
    /* creating coordinator thread */
    pthread_create(&coordinator_thread, NULL, coordinator_thread_func, NULL);
   
    int l=0;
    /* creating tutor threads */
    for(l=0; l<num_tutors; l++){
        pthread_create(&tutor_threads[l], NULL, tutor_thread_func, (void*)(intptr_t)l);
    }

    int m=0;
    /* creating student threads */
    for(m=0;m<num_students;m++){
        pthread_create(&student_threads[m], NULL, student_thread_func,(void*)(intptr_t)m);
    }

    currentNoOfTutoring = 0, totalTutored = 0 ;
    /* joining all threads */
      pthread_join(coordinator_thread, NULL);

    int p=0;
    for(p=0; p<num_students; p++){
        pthread_join(student_threads[p], NULL);
    }
    int r=0;
    for (r = 0; r < num_tutors; r++)
    {
        pthread_join(tutor_threads[r], NULL);
    }

return 0;
}
