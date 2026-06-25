# OS Project – Graph Traffic Simulation

## Milestone 1
**Compile:** `make milestone1`  
**Run:** `./dijkstra <file_name>`  
**Description:** Reads a directed weighted graph from a file and represents it in memory using an adjacency matrix. Implements Dijkstra's algorithm to find the shortest path between two nodes and prints the path and total weight to the terminal.

## Milestone 2
**Compile:** `make milestone2`  
**Run:** `./sim <file_name>`  
**Description:** Opens a raylib window and displays the graph visually. Nodes are drawn as colored circles, edges as directional arrows with weight labels. The shortest path found by Dijkstra is highlighted in red, with the source node in green and destination in orange.

## Milestone 3
**Compile:** `make milestone3`  
**Run:** `./sim <file_name>`  
**Description:** Adds a moving entity (purple circle) that travels along the Dijkstra shortest path. A PLAY/STOP button controls the animation. Edge traversal is divided into W discrete jumps (weight W × 300 ms each). The entity waits 1 full second at every intermediate node. When the destination is reached, a "Destination reached!" banner is displayed. Pressing the button again resets the animation.

## Milestone 4
**Compile:** `make milestone4`
**Run:** `./sim <file_name>`
**Description:**
Extends the graph simulation to support multiple travelers moving simultaneously along their shortest paths. The parent process reads the graph and traveler definitions from the input file, computes the shortest path for each traveler using Dijkstra's algorithm, and creates a dedicated child process for every traveler using `fork()`. Each child process prints a startup message containing its PID and remains alive while its corresponding traveler is active. The parent process manages the GUI, displaying all travelers concurrently with unique colors and animating their movement across the graph. When a traveler reaches its destination, the parent process sends a signal to terminate the associated child process. After all journeys are completed, the parent waits for all child processes to finish before exiting the simulation.

## Milestone 5
**Compile:** `make milestone5`
**Run:** `./sim <file_name>`
**Description:**
Moves to a fully autonomous child model using IPC. Each child process now computes its own Dijkstra shortest path independently (the parent no longer passes path data to the children). As each child "travels" its path it sends a `PipeMsg` struct to the parent every time it arrives at a new node, then sleeps for the appropriate travel time before moving on. The parent process reads these messages non-blocking inside the raylib GUI loop, prints a structured log to the terminal, updates each traveler's screen position, and drives the jump-based animation. Edges already traversed by each traveler are highlighted in their color as the journey progresses. When all travelers finish, a banner is displayed in the window.

**IPC mechanism chosen: anonymous pipes (`pipe()`)**
One unidirectional pipe is created per child before `fork()`. The child writes `PipeMsg` structs (8 bytes each, safely below `PIPE_BUF`, so writes are atomic); the parent reads them with `O_NONBLOCK` each GUI frame. Pipes were chosen over shared memory because:
- They are inherently unidirectional (child → parent), which matches the data flow exactly.
- No explicit synchronization primitives (mutexes/semaphores) are needed — the pipe itself serializes messages.
- Atomic writes for small structs mean no partial-read issues, keeping the implementation simple and correct.

**Terminal log format:**
```
[PID=<pid>] arrived at node <X> | next node: <Y>
[PID=<pid>] arrived at node <X> | DESTINATION
[PID=<pid>] finished
```
All log output comes exclusively from the parent process.

## Milestone 6
**Compile:** `make milestone6` | **Run:** `./sim <file_name>`

Adds mutual exclusion: at most one traveler per node at any time. Travelers wait outside occupied nodes and enter one by one.

**Sync mechanism:** One POSIX binary semaphore per node in `mmap(MAP_SHARED|MAP_ANONYMOUS)` shared memory. Child calls `sem_wait()` before entering, `sem_post()` after leaving.

**GUI indicators:**
- 🟡 Yellow traveler = waiting outside a locked node
- 🔴 Red node = occupied (mutex held)
- 🟠 Amber node = contested (someone waiting)

**Log format:**
```
[PID=<pid>] WAITING outside node <X>
[PID=<pid>] ENTERED node <X>
[PID=<pid>] FINISHED
```

## Milestone 7
**Compile:** `make milestone7`
**Run (FCFS):** `./sim -schd fcfs <file_name>`
**Run (SJF):** `./sim -schd sjf <file_name>`

Replaces the semaphore-based mutual exclusion with parent-driven scheduling. The parent (GUI process) maintains a per-node wait queue and wakes exactly the next chosen traveler via a dedicated grant pipe. Two algorithms are supported, selectable at runtime with `-schd`:

- **FCFS (First-Come-First-Served):** Travelers enter a node in the order their request arrived at the parent. Fair, but ignores how far each traveler still has to travel.
- **SJF (Shortest-Job-First):** Among all travelers waiting for the same node, the one with the smallest remaining path weight is granted entry first. This minimizes the time until the "nearly done" traveler finishes, reducing average completion time.

**IPC mechanism:** Two anonymous pipes per traveler — a *report pipe* (child → parent, carries `PipeMsg7` structs) and a *grant pipe* (parent → child, carries a 1-byte unlock signal). Children block on the grant pipe after requesting a node; the parent unblocks exactly the chosen child when the node is free.

**GUI indicators:**
- Scheduler name shown as a centered blue banner
- Queue size badge (`q:N`) drawn above each contested node
- Yellow traveler = blocked, waiting for grant
- Red node = occupied; amber node = contested (queue > 0)

**Log format:**
```
[PID=<pid>] WAITING for node <X> (priority=<remaining_weight>)
[PID=<pid>] ENTERED node <X>
[PID=<pid>] LEAVING node <X> -> <Y>
[PID=<pid>] FINISHED
```

**Algorithm comparison (using `test4.txt`):**

`test4.txt` has three travelers that share intermediate nodes (e.g., nodes 1 and 3). With FCFS, the traveler that happened to request a shared node first gets in — regardless of how much journey they have left. With SJF, the traveler closest to their destination is always prioritized, so shorter remaining journeys finish sooner. On the same input, SJF typically reduces the total time before all travelers arrive compared to FCFS when travelers have significantly different remaining distances at the moment of contention.
