// server.c
#include <stdio.h>
#include <unistd.h>   // getpid(), getppid()
#include <pthread.h>  // pthread_self()
#include <stdlib.h>   // for exit
#include <string.h>   // for strcmp
#include "prototype_defs.h"

// Suppose we have a global or static pointer to our server queue
static MyMessageQueue* g_outgoing_queue = NULL;

// Helper function: spawns a child thread that handles a command
// (In real life, we could pass more data to the thread so it can do real work.)
void handle_command_in_thread(char* command, long client_pid) {
    pthread_t main_thread_id = pthread_self();

    // 1) Log that the main thread received the command
    printf("[Main Thread -- %lu]: Received command '%s' from the client (PID: %ld). About to create a child thread.\n",
           (unsigned long)main_thread_id, command, client_pid);


    // 1) Allocate and fill ThreadArg creating the argument to pass to the child thread
    ThreadArg* tArg = (ThreadArg*)malloc(sizeof(ThreadArg));
    if (!tArg) {
        perror("Failed to allocate ThreadArg");
        return;
    }
    memset(tArg, 0, sizeof(ThreadArg));

    strncpy(tArg->command, command, sizeof(tArg->command)-1);
    tArg->client_pid = client_pid;


    // 2) Use spawn_thread_from_pool() with tArg
    //spawn_thread_from_pool() is defined in prototype_defs.c and uses pthread_create, passes in a function that handles the work on the child threads
    pthread_t* child_tid = spawn_thread_from_pool((void*)tArg); 
    if (!child_tid) {
        fprintf(stderr, "[Main Thread -- %lu]: spawn_thread_from_pool failed!\n",
                (unsigned long)main_thread_id);
        free(tArg); // must free if the thread won't use it
        return;
    }

    // 3) Log success
    printf("[Main Thread -- %lu]: Created child thread [%lu]\n",
           (unsigned long)main_thread_id, (unsigned long)*child_tid);

    // 4) Wait for child to finish
    pthread_join(*child_tid, NULL);

    // 5) Log exit
    printf("[Main Thread -- %lu]: Child thread [%lu] is finished.\n",
           (unsigned long)main_thread_id, (unsigned long)*child_tid);

    free(child_tid); // done with that pointer
}

// The child thread might do various tasks like “register client,” “hide,” etc.
// But in our demonstration, the child_thread_notification is already printing a simple message.
// If you want more advanced logic, you'd pass an argument and handle it in the thread.

int main(void) {
    // Grab basic PIDs/threads for logging
    pid_t server_pid  = getpid();
    pid_t parent_pid  = getppid();
    pthread_t main_thread = pthread_self();

    // Print server banner
printf("|----------------------------------------------------------------------------------------------|\n"
       "|------------------------ THIS IS AN INTERPROCESS SHELL SERVER --------------------------------|\n"
       "|################# THE PARENT PROCESS (PID: %d) is running this SERVER ###################|\n"
       "|----------------------------------------------------------------------------------------------|\n",
       server_pid);
    printf("[Main Thread -- %lu]: This is the Server's Main Thread. the Parent Process is (PID: %d)...\n", (unsigned long)main_thread, parent_pid);

    // 1) Create server message queue
    g_outgoing_queue = create_custom_queue("/server_queue", 10); // referring to the exact same kernel-level message queue object as client.c
    if (!g_outgoing_queue) {
        fprintf(stderr, "[Main Thread -- %lu]: ERROR creating server queue! Exiting...\n",
                (unsigned long)main_thread);
        exit(1);
    }

    printf("[Main Thread -- %lu]: Broadcast message queue & Server message queue created. Waiting for the client messages...\n", (unsigned long)main_thread);

    // 2) Simulate waiting for commands from clients by reading from the queue in a loop
    //    maybe in a real server, this might run forever until a shutdown signal.
    while (1) {
        MyMessage incoming;
        if (dequeue_message(g_outgoing_queue, &incoming) == -1) {
            // some error or queue closed
            break;
        }

        // If command is "SHUTDOWN", break
        if (strcmp(incoming.content, "SHUTDOWN") == 0) {
            printf("[Main Thread -- %lu]: Received SHUTDOWN, cleaning up...\n", 
                   (unsigned long)main_thread);
            break;
        }

        // For everything else, spawn a child thread
        handle_command_in_thread(incoming.content, incoming.client_pid);
    }

    // 3) Destroy the queue (unlink = 1 so it disappears from the system)
    destroy_message_queue(g_outgoing_queue, 1);

    // Print final message
    printf("[Main Thread -- %lu]: Server is shutting down, all resources cleaned up.\n",
           (unsigned long)main_thread);

    return 0;
}

