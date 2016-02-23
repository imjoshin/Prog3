Thread Questions
----------------

1. What happens to a thread when it exits (i.e., calls thread_exit())? What about when it sleeps?
- When a thread calls thread_exit(), the thread is detached from the parent process and verifies the detachment. Interupts are turned off on the current processor and thread_switch is called. When a thread sleeps, it yields the CPU to another thread until it is ready to run again.

2. What function(s) handle(s) a context switch?
- When a context switch is needed, a thread calls thread_switch(). Within this function, splhigh() is called to disable interrupts, spinlock_acquire() locks the run queue, thread_make_runnable() to run the thread if it is ready, and threadlist_addtail() to add the thread to the threadlist if it should wait or is a zombie, and spinlock_release() when the context switch is complete.

3. What does it mean for a thread to be in each of the possible thread states?
- S_RUN means the the thread is currently running, S_READY means the thread is ready to run, S_SLEEP means the thread is sleeping, and S_ZOMBIE means the thread is exited but not yet deleted.

4. What does it mean to turn interrupts off? How is this accomplished? Why is it important to turn off interrupts in the thread subsystem code?
- Turning interrupts off means no interrupts will be handled until they're turned back on. This is accomplished by running the command splhigh(). Turning off interrupts is important because anything that executed while they are off is atomic on a uniprocessor. This protects the processor from undergoing multiple context switches at one time.

5. What happens when a thread wakes up another thread? How does a sleeping thread get to run again?
- When a thread wakes up another thread, thread_make_runnable() is called on the target thread which adds it to the threadlist. A sleeping thread runs again when it's state is S_READY, is next in the wait queue, and a context switch occurs.


Scheduler Questions
----------------
1. What function(s) choose(s) the next thread to run?
- schedule() chooses the next thread to run.

2. How does it (do they) pick the next thread?
- If schedule isn't implemented in thread.c, threads are chosen via the round-robin fashion.

3. What role does the hardware timer play in scheduling? What hardware independent function is called on a timer interrupt?
- When the hardware timer goes off, schedule() is called to run the next thread. schedule() and thread_consider_migration() are called periodically from the timer interupt.


Synchronization Questions
----------------
1. Describe how wchan_sleep() and wchan_wakeone() are used to implement semaphores.
- wchan_sleep() is called when a thread attempts to acquire a semaphore. If it passes through the semaphore, the thread will continue running and will decrease the count by 1. If not, it will perform context switches until the semaphores count is greater than 0. wchan_wakeone() is called when a thread releases a semaphore. The count will be incremented.

2. Why does the lock API in OS/161 provide lock_do_i_hold(), but not lock_get_holder()?
- There is no benefit to having lock_get_holder() in OS/161. This is because a thread cannot impact another process. MORE HERE

