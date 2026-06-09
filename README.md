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

