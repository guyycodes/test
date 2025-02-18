// prototype_defs.c

#include "prototype_defs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>     // O_CREAT, O_RDWR, etc.
#include <sys/stat.h>  // For mode constants
#include <errno.h>
#include <unistd.h>    // getpid, getppid
#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

/* 
   A simple global or static array to store known clients.
   hidden = 0 -> visible
   hidden = 1 -> hidden
*/
static RegisteredClient g_registeredClients[MAX_CLIENTS];
static int g_numClients = 0;

/**
 * search the array for a matching pid. If found, return a pointer to that element; otherwise return NULL
 * We do not allocate new memory for the returned pointer. 
 * We just return the address within the static array. The caller must not free it.
 */
RegisteredClient* get_client_status(pid_t client_ID)
{
    // Edge case: if client_ID == 0, bail out
    if (client_ID == 0) {
        fprintf(stderr, "get_client_status: Invalid client_ID == 0\n");
        return NULL;
    }

    // Look through our array for a matching PID
    for (int i = 0; i < g_numClients; i++) {
        if (g_registeredClients[i].pid == client_ID) {
            return &g_registeredClients[i];  // found it
        }
    }

    // Not found
    return NULL;
}

/*
* Check if status is valid (0 or 1).
* Try finding the client with get_client_status().
* If found, update that client’s hidden field.
* If not found, create a new entry (PID + hidden). 
*/
RegisteredClient* set_client_status(pid_t client_ID, int status)
{
    // Validate status
    if (status != 0 && status != 1) {
        fprintf(stderr, "set_client_status: 'status' must be 0 or 1\n");
        return NULL;
    }

    // First, see if client already exists
    RegisteredClient* rc = get_client_status(client_ID);
    if (rc) {
        // If exists, just update it
        rc->hidden = status;
        return rc;
    }

    // Otherwise, we need to add a new client entry
    if (g_numClients >= MAX_CLIENTS) {
        fprintf(stderr, "set_client_status: Reached max clients (%d). Cannot add client %ld\n",
                MAX_CLIENTS, (long)client_ID);
        return NULL;
    }

    // Insert at the end
    g_registeredClients[g_numClients].pid = client_ID;
    g_registeredClients[g_numClients].hidden = status;

    g_numClients++;

    // Return pointer to the newly added entry
    return &g_registeredClients[g_numClients - 1];
}

/**
 * Helper to remove a client from the g_registeredClients array 
 * (in case we want to free up that slot on 'EXIT' command).
 */
int remove_client_status(pid_t client_ID) 
{
    // find the index of the given client
    int idx = -1;
    for (int i = 0; i < g_numClients; i++) {
        if (g_registeredClients[i].pid == client_ID) {
            idx = i;
            break;
        }
    }
    if (idx == -1) {
        // client not found
        return -1;
    }
    // Shift subsequent entries down by 1
    for (int j = idx; j < g_numClients - 1; j++) {
        g_registeredClients[j] = g_registeredClients[j + 1];
    }
    g_numClients--;
    return 0;
}


// Helper function: This is a quick example of how to print visible clients
// (hidden == 0).
void list_visible_clients() 
{
    int visibleCount = 0;
    printf("===== Visible Clients =====\n");
    for (int i = 0; i < g_numClients; i++) {
        if (g_registeredClients[i].hidden == 0) {
            printf(" -> Client PID: %ld\n", (long)g_registeredClients[i].pid);
            visibleCount++;
        }
    }
    if (visibleCount == 0) {
        printf("All Clients Are Hidden...\n");
    }
    printf("===========================\n");
}

/**
 * Creates or opens a POSIX message queue and returns a pointer to MyMessageQueue.
 */
MyMessageQueue* create_custom_queue(char* name, long max_messages) {
    // Allocate our "queue object"
    MyMessageQueue* myObj = (MyMessageQueue*)malloc(sizeof(MyMessageQueue));
    if (!myObj) {
        perror("malloc for create_custom_queue failed");
        return NULL;
    }
    memset(myObj, 0, sizeof(MyMessageQueue));

    // Copy the queue name into the struct
    strncpy(myObj->queue_name, name, sizeof(myObj->queue_name) - 1);

    // Configure the message queue attributes
    myObj->attributes.mq_flags   = 0;                 // 0: default blocking
    myObj->attributes.mq_maxmsg  = max_messages;      // how many msgs allowed
    myObj->attributes.mq_msgsize = sizeof(MyMessage); // size of each message
    myObj->attributes.mq_curmsgs = 0;                 // current # of msgs

    // Open (or create) the message queue
    // O_CREAT => create if doesn't exist
    // O_RDWR  => open for both read and write
    // 0644   => permissions (owner read/write, others read)
    // Because we use the same name ("/server_queue") and the same O_CREAT | O_RDWR flags 
    // (or at least O_WRONLY for the client, O_RDONLY for the server), 
    // processes thatshare the same name can send and receive on this queue.
    // returns a message queue descriptor that points to the shared kernel queue.
    mqd_t mqd = mq_open(myObj->queue_name, O_CREAT | O_RDWR, 0644, &myObj->attributes);
    if (mqd == (mqd_t)-1) {
        perror("mq_open failed");
        free(myObj);
        return NULL;
    }

    myObj->msg_queue_descriptor = mqd;
    return myObj;
}

/**
 * Closes and optionally unlinks the queue, then frees the struct.
 */
void destroy_message_queue(MyMessageQueue* myObj, int unlink_on_destroy) {
    if (!myObj) {
        return;
    }
    // Close the queue descriptor
    if (mq_close(myObj->msg_queue_descriptor) == -1) {
        perror("mq_close failed");
    }

    // Optionally remove the queue from the system
    if (unlink_on_destroy) {
        if (mq_unlink(myObj->queue_name) == -1) {
            perror("mq_unlink failed");
        }
    }

    free(myObj);
}

/**
 * Enqueues (sends) a message into the queue.
 * If the queue is full, it returns -1 and sets errno = EAGAIN
 */
int enqueue_message(MyMessageQueue* myObj, MyMessage* msg) {
    if (!myObj || !msg) {
        errno = EINVAL;
        return -1;
    }
    // mq_send blocks if the queue is full (and mq_flags=0), or returns EAGAIN if non-blocking
    // client uses mq_send, can now store in the kernel’s queue  in our implementation its named "/server_queue"
    if (mq_send(myObj->msg_queue_descriptor, (char*)msg, sizeof(MyMessage), 0) == -1) {
        if (errno == EAGAIN) {
            // queue is full
            printf("Queue is full");
        } else {
            perror("mq_send (non-blocking) failed");
        }
        return -1;
    }
    return 0;
}

/**
 * Dequeues (receives) a message from the queue. Blocks until a message is available.
 */
int dequeue_message(MyMessageQueue* myObj, MyMessage* outMsg) {
    if (!myObj || !outMsg) {
        errno = EINVAL;
        return -1;
    }
    // mq_receive blocks if queue is empty (and mq_flags=0).
    ssize_t bytesRead = mq_receive(myObj->msg_queue_descriptor,
                                   (char*)outMsg,
                                   sizeof(MyMessage),
                                   NULL);
    if (bytesRead < 0) {
        perror("mq_receive failed");
        return -1;
    }
    return 0;
}

/**
 * child_thread_func()
 * Thread function that logs its own ID.
 */
void* child_thread_func(void* arg) {
    printf("[Child Thread -- %lu]: Hello from the child_thread.\n",
        (unsigned long)pthread_self());
    // cast and retrieve data
    ThreadArg* data = (ThreadArg*)arg;
    if (!data) {
        pthread_exit(NULL);
    }
    // get the actual TID
    pthread_t tid = pthread_self();

    // now print with real TID
    printf("[Child Thread * %lu]: Handling command '%s' for client PID=%ld\n",
           (unsigned long)tid, data->command, data->client_pid);

    // In a real program you could do your actual child thread logic here (shell exec and user-defined commands)...
    // child-thread-specific logic (HIDE, UNHIDE, etc.)
    if (strcmp(data->command, "REGISTER") == 0) {
        set_client_status(data->client_pid, 0);
        printf("[Child Thread -- %lu]: Registered client %ld (visible=0)\n",
               (unsigned long)tid, (long)data->client_pid);
    }
    else if (strcmp(data->command, "LIST") == 0) {
        // etc.
        list_visible_clients(); 
        printf("[Child Thread -- %lu]: Done listing.\n", (unsigned long)tid);
    }
    else if (strcmp(data->command, "HIDE") == 0) {
        set_client_status(data->client_pid, 1);
        printf("[Child Thread -- %lu]: Client %ld is now hidden.\n",
               (unsigned long)tid, (long)data->client_pid);
    }
    else if (strcmp(data->command, "UNHIDE") == 0) {
        set_client_status(data->client_pid, 0);
        printf("[Child Thread -- %lu]: Client %ld is now visible.\n",
               (unsigned long)tid, (long)data->client_pid);
    }
    else if (strcmp(data->command, "EXIT") == 0) {
        remove_client_status(data->client_pid);
        printf("[Child Thread -- %lu]: Cleaned up client %ld.\n",
               (unsigned long)tid, (long)data->client_pid);
    }
    else if (strcmp(data->command, "exit") == 0) {
        printf("[Child Thread -- %lu]: Ignoring lowercase 'exit'.\n",
               (unsigned long)tid);
    }
    else {
        // Possibly a shell command => fork/exec with 3-sec timeout
        printf("[Child Thread -- %lu]: Attempting shell command '%s'\n",
               (unsigned long)tid, data->command);
         shell_exec_with_timeout(data->command);
    }

    free(data);  // free the ThreadArg
    pthread_exit(NULL);
}


/**
 * spawn_thread_from_pool()
 * Creates a new thread that runs any function.
 * Returns a pointer to a newly allocated pthread_t.
 */
pthread_t* spawn_thread_from_pool(void* notification) {
    pthread_t* new_tid = (pthread_t*)malloc(sizeof(pthread_t));
    if (!new_tid) {
        perror("malloc for new thread failed");
        return NULL;
    }

    int ret = pthread_create(new_tid, NULL, child_thread_func, notification); //  pass the notification pointer to child_thread_func()
    if (ret != 0) {
        perror("pthread_create failed");
        free(new_tid);
        return NULL;
    }

    // If you want the thread to be detached (run independently):
    // pthread_detach(*new_tid);

    return new_tid;
}

/**
 * create_client()
 * For demonstration, prints info about the "Client" creation.
 * In a real scenario, you might set up your client resources here, spawn threads, etc.
 */
void create_client(pid_t parent_pid, pthread_t main_thread) {
    pid_t my_pid = getpid();           // The current process's PID
    pid_t real_parent = getppid();     // Usually the shell or launching process

    printf("|################## I am the Parent Process (PID: %d) running this Client #################|\n",
           my_pid);
    printf("[Main Thread -- %lu]: I am the Client's Main Thread. My Parent Process is (PID: %d)...\n",
           (unsigned long)main_thread, parent_pid);

    // Example: Create a child thread to show the usage
    pthread_t* child_tid = spawn_thread_from_pool(NULL);
    if (child_tid) {
        printf("[Main Thread -- %lu]: Successfully created child thread [%lu] in client.\n",
               (unsigned long)pthread_self(), (unsigned long)(*child_tid));
    }

    // In a real-world scenario, store child_tid somewhere or join it later
    // For demo, let's just join here
    pthread_join(*child_tid, NULL);
    free(child_tid);

    printf("[Main Thread -- %lu]: create_client() completed. (Real parent was PID: %d)\n",
           (unsigned long)pthread_self(), real_parent);
}


/**
 * create_server()
 * Similar concept, logs creation, spawns threads, etc.
 */
void create_server(pid_t parent_pid, pthread_t main_thread) {
    pid_t my_pid = getpid();           
    pid_t real_parent = getppid();     

    printf("|################### I am the PARENT PROCESS (PID: %d) running this SERVER ##################|\n",
           my_pid);
    printf("[Main Thread -- %lu]: I am the Server's Main Thread. My Parent Process is (PID: %d)...\n",
           (unsigned long)main_thread, parent_pid);

    // Example: spawn a thread
    pthread_t* child_tid = spawn_thread_from_pool(NULL);
    if (child_tid) {
        printf("[Main Thread -- %lu]: Successfully created child thread [%lu] in server.\n",
               (unsigned long)pthread_self(), (unsigned long)(*child_tid));
    }

    // Join (or detach) the child thread
    pthread_join(*child_tid, NULL);
    free(child_tid);

    printf("[Main Thread -- %lu]: create_server() completed. (Real parent was PID: %d)\n",
           (unsigned long)pthread_self(), real_parent);
}

/**
 * shell_exec_with_timeout()
 * function to fork/exec a shell command, with a 3-second limit.
 */
void shell_exec_with_timeout(char *cmd)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return;
    }
    else if (pid == 0) {
        // Child process: exec the command in /bin/bash
        execlp("/bin/bash", "bash", "-c", cmd, (char *)NULL);
        // If execlp fails:
        perror("execlp");
        exit(EXIT_FAILURE);
    }
    else {
        // Parent process: wait up to 3 seconds, kill if it’s still running
        time_t start = time(NULL);
        int status;
        while (1) {
            pid_t result = waitpid(pid, &status, WNOHANG);
            if (result == -1) {
                perror("waitpid");
                return;
            }
            else if (result == 0) {
                // child still running
                if (difftime(time(NULL), start) > 3.0) {
                    // Timeout -> kill child
                    kill(pid, SIGKILL);
                    printf("[shell_exec_with_timeout]: Command '%s' timed out and was killed.\n", cmd);
                    break;
                }
                // Sleep a bit before checking again
                usleep(100000); // 100ms
            }
            else {
                // Child finished normally
                printf("[shell_exec_with_timeout]: Command '%s' completed.\n", cmd);
                break;
            }
        }
    }
}
