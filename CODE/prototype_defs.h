// prototype_defs.h

#ifndef PROTOTYPE_DEFS_H
#define PROTOTYPE_DEFS_H

#include <mqueue.h>
#include <sys/types.h>
#include <pthread.h>  // for pthread_t

#define MAX_CLIENTS 50 // arbitrary limit

/**
 * This struct holds a single message's content.
 */
typedef struct {
    long client_pid;        // store which client sent the message
    char content[256];      // the actual message text
} MyMessage;

/**
 * This struct represents the message queue.
 */
typedef struct {
    mqd_t msg_queue_descriptor;  // POSIX message queue descriptor
    char queue_name[128];        // the name used in mq_open
    struct mq_attr attributes;   // holds things like max messages, msg size, etc.
} MyMessageQueue;

/**
 * Store the client states (hidden/not hidden)
 * hidden = 0 means visible, hidden = 1 means hidden
 */
typedef struct {
    long pid;
    int hidden; 
} RegisteredClient;

/*
 * Pass the necessary information (command string, client PID) to get the child thread
*/
typedef struct {
    char command[256];
    long client_pid;
} ThreadArg;

/* =========================
   Function Prototypes
   ========================= */
RegisteredClient* get_client_status(pid_t client_ID);
RegisteredClient* set_client_status(pid_t client_ID, int status);
int remove_client_status(pid_t client_ID);
void list_visible_clients(void);
void* child_thread_func(void* arg);

/**
 * Creates a POSIX message queue with the given name and max capacity.
 * Returns a pointer to a dynamically allocated MyMessageQueue on success, or NULL on failure.
 */
MyMessageQueue* create_custom_queue(char* name, long max_messages);

/**
 * Closes and optionally unlinks (removes) the message queue, then frees the MyMessageQueue struct.
 */
void destroy_message_queue(MyMessageQueue* myObj, int unlink_on_destroy);

/**
 * Enqueues (sends) a message into the queue.
 * Returns 0 on success, -1 on failure.
 */
int enqueue_message(MyMessageQueue* myObj, MyMessage* msg);

/**
 * Dequeues (receives) a message from the queue. Blocks until a message is available.
 * Returns 0 on success, -1 on failure.
 *
 * NOTE: Only the calling thread is blocked, not the entire process. The thread
 * remains in a suspended state until:
 *   - A message becomes available (another process/thread calls enqueue_message),
 *   - The thread receives a signal,
 *   - or the queue is deleted (mq_unlink).
 */
int dequeue_message(MyMessageQueue* myObj, MyMessage* outMsg);


/**
 * Creates a client process - logs the setup like the professors code.
 * @param parent_pid    The parent's PID (launching or shell process)
 * @param main_thread   The client's main thread ID
 */
void create_client(pid_t parent_pid, pthread_t main_thread);

/**
 * Creates a server process - logs the setup like the professors code.
 * @param parent_pid    The parent's PID (shell or launching process)
 * @param main_thread   The server's main thread ID
 */
void create_server(pid_t parent_pid, pthread_t main_thread);

/**
 * Notifies the user about spawned child threads (thread function).
 */
void* child_thread_notification(void* arg);

 /**
 * Spawns a new thread from a "pool" (or simply creates one).
 *  @param notification  An argument pointer passed into the child thread function.
 *  Process is the parent container:
 *  NOTE: Every thread MUST belong to a process
 *        The process provides the memory space, file descriptors, and other resources
 *        TThe process is the "house" that threads live in (Process Control Blocks *PCB's)
 */
pthread_t* spawn_thread_from_pool(void* notification);


#endif // PROTOTYPE_DEFS_H