# GTthread
User thread library (CS 6210, Advanced Operating System, Georgia Tech)

# Linux Platform
Linux mint 3.16.0-28-generic #38-Ubuntu SMP

# Preemptive Scheduler
I keep a linked list of all the runnable threads with the invariant that head of the list always points to the thread executing on CPU at this point of time. Whenever the timer interrupt comes, the head is put at the tail and the next thread (node) in the linked list is scheduled to execute on CPU.

# How to Run
* To compile the code, simply run `make`
* It will create executable `phil_test` for philosopher example and `gtthread.a` library
* Ignore the warnings
* Link your own program with `gtthread.a` and include `gtthread.h`

# Prevent Deadlocks in Dining Philosophers Example
Deadlock, in this example, is avoided by breaking the symmetry. Philosophers with even id tries to acqiure right chopstick first whereas philosophers with odd id tries to acquire left chopstick first.
