# Elevator

[Description]

## Group Members
- **Bridget Gizzi**: bag23c@fsu.edu
- **Jack Waite**: jrw24a@fsu.edu
- **Jake Bertorello**: JMB23A@fsu.edu
## Division of Labor

### Part 1: System Call Tracing
- **Responsibilities**: ran strace on empty.c and part1.c
- **Assigned to**: Bridget Gizzi

### Part 2: Timer Kernel Module
- **Responsibilities**: make module display currrent & elapsed time 
- **Assigned to**: Bridget Gizzi

### Part 3a: Adding System Calls
- **Responsibilities**: Adding 3 syscalls to the kernel
- **Assigned to**: Jake Bertorello

### Part 3b: Kernel Compilation
- **Responsibilities**: Compiling and instlling kernel verison 6.16 onto VM
- **Assigned to**: Jake Bertorello

### Part 3c: Threads
- **Responsibilities**: Added the elevator's background kthread that runs the elevator loop until the module is unloaded
- **Assigned to**: Jack Waite

### Part 3d: Linked List
- **Responsibilities**: Added linked lists for tracking passengers waiting on each floor and passengers currently on the elevator
- **Assigned to**: Jack Waite

### Part 3e: Mutexes
- **Responsibilities**: Added mutex locking around all shared elevator state so the background thread and system calls don't conflict with each other
- **Assigned to**: Jack Waite

### Part 3f: Scheduling Algorithm
- **Responsibilities**:  Added the elevator movement logic - starting, stopping, unloading, etc
- **Assigned to**: Jack Waite

## File Listing
```
elevator/
├── Makefile
├── part1/
│   ├── empty.c
│   ├── empty.trace
│   ├── part1.c
│   ├── part1.trace
│   └── Makefile
├── part2/
│   ├── src/
│   │   └── my_timer.c
│   └── Makefile
├── part3/
│   ├── src/
│   │   └── elevator.c
│   ├── tests/
│   ├── Makefile
│   └── sys_call.c
├── Makefile
└── README.md

```
# How to Compile & Execute

### Requirements
- **Kernel**: Linux kernel 6.16.0 with custom syscalls (548, 549, 550).
- **Compiler**: e.g., `gcc` for C/C++, `rustc` for Rust.
- **Dependencies**: List any libraries or frameworks necessary (rust only).

## Part 1

### Compilation
For a C/C++ example:
```bash
cd part1/
make
```
This will build the executable in ...
### Execution
```bash
strace -o empty.trace ./empty
strace -o part1.trace ./part1
wc -l empty.trace part1.trace
```
This will run the program ...

## Part 2

### Compilation
For a C/C++ example:
```bash
cd part2/
make
```
This will build the executable in ...
### Execution
```bash
sudo insmod my_timer.ko
cat /proc/timer
sleep 1
cat /proc/timer
sleep 3
cat /proc/timer
sleep 5
cat /proc/timer
sudo rmmod my_timer
```
This will run the program ...

## Part 3

### Compilation
For a C/C++ example:
```bash
cd part3/
make
```
This will build the executable in ...
### Execution
```bash
sudo insmod elevator.ko
cat /proc/elevator
cd tests/elevator-test
./consumer --start
./producer 100
watch -n 1 cat /proc/elevator
# Ctrl C to exit
./consumer --stop
sudo rmmod elevator
```
This will run the program ...

## Development Log
Each member records their contributions here.

### Bridget Gizzi

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-03-03 | part 1                 |
| 2026-03-04 | part 2                 |
| 2026-03-05 | makefile               |

### Jake Bertorello

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-03-22 | add syscalls           |
| 2026-03-22 | kernel compilation     |
| 2026-03-23 | update readme          |
| 2026-03-23 | big fixes              |


### Jack Waite

| Date       | Work Completed / Notes |
|------------|------------------------|
| 2026-03-21 | Completed Section 3c   |
| 2026-03-22 | Completed Section 3d   |
| 2026-03-22 | Completed Section 3e   |
| 2026-03-22 | Completed Section 3f   |
| 2026-03-23 | Updated documentation  |

[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/fHKNR9S-)
