// client.c
#include <stdio.h>
#include <unistd.h>   // getpid(), getppid()
#include <pthread.h>  // pthread_self()
#include <stdlib.h>   // for exit
#include <string.h>   // for strcmp
#include "prototype_defs.h"

static MyMessageQueue* g_incoming_queue = NULL;

void* shutdown_listener_thread(void* arg) {
    // Example: in a real design,we could create a separate broadcast queue just for SHUTDOWN
    // For simplicity, we can pretend we read from the same queue or do something else.
    // We'll just do a placeholder message here:
    printf("[Shutdown Listener Thread -- %lu]: Listening for SHUTDOWN (not implemented in detail) ...\n",
           (unsigned long)pthread_self());

    // (Implementation detail: might do mq_receive on a broadcast queue)
    // For demonstration, we just sleep or do a loop:
    while (1) {
        sleep(2); // Pause the execution of this thread for 2 seconds each iteration
        // If you detect "SHUTDOWN" from server, break
        // break;
    }
    return NULL;
}

int main(void) {
    pid_t client_pid = getpid();
    pid_t parent_pid = getppid();
    pthread_t main_thread = pthread_self();

    // Print banner similar to instructor's reference
    printf("|----------------------------------------------------------------------------------------------|\n"
           "|------------------------ THIS IS AN INTERPROCESS SHELL SERVER --------------------------------|\n"
           "|################# THE PARENT PROCESS (PID: %d) is running this SERVER ###################|\n"
           "|----------------------------------------------------------------------------------------------|\n"
           , client_pid);
    printf("[Main Thread -- %lu]: This is the Client's Main Thread. My Parent Process is (PID: %d)...\n", (unsigned long)main_thread, parent_pid);

    // 1) Create a "child thread" to listen for SHUTDOWN
    pthread_t shutdown_listener;
    int ret = pthread_create(&shutdown_listener, NULL, shutdown_listener_thread, NULL);
    if (ret == 0) {
        printf("[Main Thread -- %lu]: Created a Child Thread [%lu] for SHUTDOWN broadcast message...\n",
               (unsigned long)main_thread, (unsigned long)shutdown_listener);
        // Force the main thread to sleep momentarily:
        // This gives the child thread a chance to run and print before we do our prompt.
        usleep(50000);  // 50ms, for example
    }

    // 2) Create (or open) the same queue as server so we can send commands to server
    g_incoming_queue = create_custom_queue("/server_queue", 10); // referring to the exact same kernel-level message queue object as server.c
    if (!g_incoming_queue) {
        fprintf(stderr, "[Main Thread -- %lu]: ERROR opening server queue!\n",
                (unsigned long)main_thread);
        exit(1);
    }

    // 1) Send "REGISTER" to server so it can track client as visible
    MyMessage regMsg;
    regMsg.client_pid = client_pid;
    snprintf(regMsg.content, sizeof(regMsg.content), "REGISTER");
    enqueue_message(g_incoming_queue, &regMsg);

    printf("[Main Thread -- %lu]: Client initialized. Enter commands (type 'EXIT' to quit)...\n\n",
           (unsigned long)main_thread);

    // 3) Simple REPL (read-eval-print loop): read user input, send messages to server
    char input[256];
    char prompt[256] = "Enter Command";
    while (1) {

        printf("%s> ", prompt);
        fflush(stdout);

        if (!fgets(input, sizeof(input), stdin)) {
            // user closed input (Ctrl+D?)
            break;
        }
        // remove trailing newline
        char* nl = strchr(input, '\n');
        if (nl) *nl = '\0';

        // If the user typed nothing (just Enter), skip or handle as invalid
        if (strlen(input) == 0) {
            printf("Invalid input. Please enter a valid command.\n");
            continue;
        }

        // --- Parse the input to check commands ---
        // One simple approach: split at first space
        // "CHPT myPrompt" -> cmd="CHPT", arg="myPrompt"
        char *cmd = strtok(input, " ");    // first token
        char *arg = strtok(NULL, "");      // the rest of the line

        if (strcmp(input, "EXIT") == 0) {
            // Send EXIT to server, then break
            MyMessage msg;
            msg.client_pid = client_pid;
            snprintf(msg.content, sizeof(msg.content), "%s", "EXIT");

            enqueue_message(g_incoming_queue, &msg);

            printf("[Main Thread -- %lu]: Exiting on user command...\n", (unsigned long)main_thread);
            break;
        }else if(strcmp(input, "CHPT") == 0){
                        // 2) CHPT changes the client's prompt (local only)
            if (!arg) {
                // If user typed just "CHPT" with no argument
                printf("Usage: CHPT <new_prompt>\n");
            } else {
                // Copy the argument into 'prompt'
                strncpy(prompt, arg, sizeof(prompt) - 1);
                prompt[sizeof(prompt) - 1] = '\0'; // ensure null termination
                printf("Prompt changed to: '%s'\n", prompt);
            }
            continue;
        }

        // else send it as is
        MyMessage msg;
        msg.client_pid = client_pid;
        snprintf(msg.content, sizeof(msg.content), "%s", input);

        enqueue_message(g_incoming_queue, &msg);

        // For demonstration, pretend we read back a response from the server on a separate queue
        // In a real system, you'd likely have a separate client-specific queue to read server's response
        // We'll just print a placeholder
        // printf("[Main Thread -- %lu]: (Pretend) Received server response\n", (unsigned long)main_thread);
        printf("======================================================\n");
        // e.g. parse server response if you had a client queue
    }

    // 4) Clean up
    if (shutdown_listener) {
        // In a real scenario, you might signal the shutdown listener or kill it
        pthread_cancel(shutdown_listener);
        pthread_join(shutdown_listener, NULL);
    }

    destroy_message_queue(g_incoming_queue, 0); // don't unlink
    printf("[Main Thread -- %lu]: Resource cleanup complete. Shutting down...\n",
           (unsigned long)main_thread);

    return 0;
}
