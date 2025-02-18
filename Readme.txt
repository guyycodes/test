#####################################
# Group Members: Guy Morgan Beals   #
# Group: RVC                        #
# PID: 6432286                      #
# gbeal001@fiu.edu                  #
#####################################

## Overview

This project implements a **multi-threaded, asynchronous Client/Server** application using **POSIX Message Queues** for Inter-Process Communication (IPC). The **server** acts like a Linux shell, processing both **shell commands** and **custom user-defined commands** (e.g., `LIST`, `HIDE`, `EXIT`, `CHPT`, etc.). The **client** sends commands to the server and receives responses. The server also maintains each client’s visibility status via a localized global array (not shared across processes).

---

## Implementation Approach

- **IPC Mechanism**:  
  - We chose **POSIX Message Queues** because they allow flexible, asynchronous message passing between client and server. They also simplify concurrency when multiple clients are present, compared to implementing our own shared memory coordination.
  - Ensure you have the Posix message queue installed. 
  ```
  sudo apt install libmqueue-dev
  ```
  
- **Client**:  
  - Registers with the server upon startup.  
  - Reads user commands in a loop (REPL).  
  - Sends commands to the server via the message queue.  
  - Supports special commands like `CHPT` (changing prompt locally), `EXIT` (disconnect), and normal shell commands (forwarded to server for execution).

- **Server**:  
  - Spawns threads to handle each incoming command.  
  - Uses a local global array to keep track of each client’s “hidden” or “visible” state.  
  - Can process shell commands via fork/exec with a 3-second timeout.

---

## Prerequisites (Static Linking)

Since these executables are **statically linked**, you may need certain packages installed:

```bash
sudo apt-get update
sudo apt-get install -y glibc-static libpthread-stubs0-dev musl-tools 

# Also ensure you have the messaging queue installed
 sudo apt install libmqueue-dev

# Build and Run Instructions
## Navigate to the CODE directory (or wherever your source files and Makefile reside).

1) Compile the programs by running:
```
make
```
This should produce two statically-linked executables: server and client.

2) Start the Server:
```
./server
```
The server must be running before any client can connect.
Open a new terminal & Start the Client:
```
./client
```
The client will register itself with the server, then present a prompt for entering commands.
Commands (recognized by the server):

LIST: Lists all visible (unhidden) clients.

HIDE: Hides the current client from the LIST.

UNHIDE: Makes the current client visible again.

CHPT <new_prompt>: Changes the client’s local prompt (e.g., CHPT MyPrompt).
(Note: This is handled locally by the client—no server action required.)

EXIT: Tells the server the client is leaving, then quits the client.

SHUTDOWN: If issued by the server, causes all clients to terminate.
(Clients shouldn’t send SHUTDOWN—it’s server-initiated only.)

Any other text is treated as a shell command, which the server will attempt to run in a child process (with a 3-second timeout).

Shutting Down:

1) From the Server: Type or enqueue a SHUTDOWN command to broadcast a shutdown to all clients, then terminate.
2) From the Client: Type EXIT or press Ctrl+C.
3) If the server exits first (e.g., Ctrl+C), clients may detect the missing queue or not receive further communication.
4) Justification for POSIX Message Queues
5) Flexibility: The queue can handle multiple messages without complex manual synchronization.
6) Asynchronous: The client and server can run independently, sending and receiving messages without blocking each other.
7) Ease of Use: POSIX queues provide a simpler API (e.g., mq_open, mq_send, mq_receive) compared to setting up shared memory with semaphores.
8) Additionally, storing client status (hidden or visible) in a local array on the server side is straightforward and does not require special synchronization objects outside the server. Thus, the chosen approach meets the assignment requirements efficiently.

# Justification for POSIX Message Queues
Flexibility: The queue can handle multiple messages without complex manual synchronization.
Asynchronous: The client and server can run independently, sending and receiving messages without blocking each other.
Ease of Use: POSIX queues provide a simpler API (e.g., mq_open, mq_send, mq_receive) compared to setting up shared memory with semaphores.

Additionally, storing client status (hidden or visible) in a local array on the server side is straightforward and does not require special 
synchronization objects outside the server. Thus, the chosen approach meets the assignment requirements efficiently.
