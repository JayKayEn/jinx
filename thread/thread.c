#include <lib.h>
#include <x86.h>
#include <kmm.h>
#include <pmm.h>
#include <vmm.h>
#include <thread.h>
#include <threadlist.h>
#include <cpu.h>
#include <proc.h>
#include <wchan.h>
#include <array.h>
#include <semaphore.h>
#include <errno.h>
#include <switchframe.h>
#include <debug.h>
#include <syscall.h>

DEFARRAY(thread, );

#define THREAD_STACK_MAGIC 0xDEADFACE

static
void
thread_initstack(void* stack_addr) {
    ((uint32_t*) stack_addr)[0] = THREAD_STACK_MAGIC;
    ((uint32_t*) stack_addr)[1] = THREAD_STACK_MAGIC;
    ((uint32_t*) stack_addr)[2] = THREAD_STACK_MAGIC;
    ((uint32_t*) stack_addr)[3] = THREAD_STACK_MAGIC;
}

static
void
thread_checkstack(struct thread* thread) {
    assert(thisthread == bootthread || thread->stack != NULL);
    assert(((uint32_t*) thread->stack)[0] = THREAD_STACK_MAGIC);
    assert(((uint32_t*) thread->stack)[1] = THREAD_STACK_MAGIC);
    assert(((uint32_t*) thread->stack)[2] = THREAD_STACK_MAGIC);
    assert(((uint32_t*) thread->stack)[3] = THREAD_STACK_MAGIC);
}

/*
 * Marks next avaialble stack_addr in registrar's bitmap as in use.
 * Returns this stack_addr.
 */
void*
thread_getstack(void) {
    void* stack_addr = page_get();

    if (stack_addr == NULL)
        panic("OOM: no memory available for stack creation");

    thread_initstack(stack_addr);

    return stack_addr;
}

// this makes a stack_addr available again to the system
void
thread_returnstack(void* stack_addr) {
    assert(stack_addr != NULL);

    page_return(stack_addr);
}

struct thread*
thread_create(const char* name) {
    struct thread* thread = kmalloc(sizeof(struct thread));
    if (thread == NULL)
        return NULL;

    assert(name != NULL);
    thread->name = strdup(name);
    if (thread->name == NULL) {
        kfree(thread);
        return NULL;
    }

    thread->wchan_name = "NEW";
    thread->state = S_READY;

    threadlistnode_init(&thread->listnode, thread);

    thread->cpu = NULL;
    thread->proc = NULL;

    thread->stack = NULL;

    thread->in_interrupt = false;

    thread->rval = -1;
    thread->parent = NULL;
    thread->psem = NULL;
    thread->csem = NULL;

    return thread;
}

int
thread_fork(const char* name, struct thread** thread_out,
        struct proc* proc, int (*entrypoint)(void*, unsigned long),
        void* data1, unsigned long data2) {

    bool on = cli();

    struct thread* newthread = thread_create(name);
    if (newthread == NULL)
        return ENOMEM;

    newthread->stack = thread_getstack();
    if (newthread->stack == NULL) {
        thread_destroy(newthread);
        return ENOMEM;
    }
    thread_checkstack(newthread);

    newthread->cpu = thiscpu;

    if (thread_out != NULL) {
        *thread_out = newthread;
        newthread->parent = thisthread;

        newthread->csem = semaphore_create(name, 0);
        if (newthread->csem == NULL) {
            thread_destroy(newthread);
            return ENOMEM;
        }

        newthread->psem = semaphore_create(name, 0);
        if (newthread->psem == NULL) {
            semaphore_destroy(newthread->csem);
            thread_destroy(newthread);
            return ENOMEM;
        }
    }

    int result = proc_addthread(proc ? proc : thisthread->proc, newthread);
    if (result) {
        if (thread_out != NULL) {
            semaphore_destroy(newthread->psem);
            semaphore_destroy(newthread->csem);
        }
        thread_destroy(newthread);
        return result;
    }

    switchframe_init(newthread, entrypoint, data1, data2);

    thread_make_runnable(newthread, false);

    ifx(on);

    return 0;
}

void
thread_switch(threadstate_t newstate, struct wchan* wc, struct spinlock* lk) {
    assert(cli() == false);  // interrupts are off
    assert(thiscpu->thread == thisthread);
    assert(thisthread->cpu == thiscpu->self);

    // TODO: revise the following comment because I no longer use stackreg:
    // // we may already have stackreg's spinlock and we can't reacquire it so skip
    // // exorcising if going to sleep
    if (newstate != S_SLEEP)
        thread_exorcise();

    /*
     * If we're idle, return without doing anything. this happens
     * when the timer interrupt interrupts the idle loop.
     */
    if (thiscpu->status == CPU_IDLE)
        return;

    thread_checkstack(thisthread);

    // asm volatile ("mfence" ::: "memory");  // uncomment?

    spinlock_acquire(&thiscpu->active_threads_lock);

    switch (newstate) {
        case S_RUN:
            panic("Illegal S_RUN in thread_switch\n");
        case S_READY: {
            // if thisthread is the only thread, just return
            if (threadlist_isempty(&thiscpu->active_threads)) {
                spinlock_release(&thiscpu->active_threads_lock);
                return;
            }
            thread_make_runnable(thisthread, true /* holding_lock */);
            break;
        }
        case S_SLEEP:
            thisthread->wchan_name = wc->wc_name;
            threadlist_addtail(&wc->wc_threads, thisthread);
            spinlock_release(lk);
            break;
        case S_ZOMBIE:
            thisthread->wchan_name = "ZOMBIE";
            threadlist_addtail(&thiscpu->zombie_threads, thisthread);
            break;
        default:
            panic("unknown thread state in thread_switch: %u\n", newstate);
    }
    thisthread->state = newstate;

    thiscpu->status = CPU_IDLE;
    struct thread* next = NULL;
    do
        if ((next = threadlist_remhead(&thiscpu->active_threads)) == NULL) {
            spinlock_release(&thiscpu->active_threads_lock);
            cpu_idle();
            spinlock_acquire(&thiscpu->active_threads_lock);
        }
    while (next == NULL);
    thiscpu->status = CPU_STARTED;

    next->wchan_name = NULL;
    next->state = S_RUN;

    lcr3(PADDR(next->proc->page_directory));

    spinlock_release(&thiscpu->active_threads_lock);

    switchframe_switch((thisthread = next)->context);

    panic("switchframe_switch returned");
}

void
thread_schedule(void) {
    assert(cli() == false);  // interrupts are off
    thread_switch(S_READY, NULL, NULL);
}

void
thread_start(int (*entrypoint)(void* data1, unsigned long data2),
               void* data1, unsigned long data2) {
    assert(cli() == false);  // interrupts are off
    assert(thisthread != NULL);

    /* Clear the wait channel and set the thread state. */
    thisthread->wchan_name = NULL;
    thisthread->state = S_RUN;

    thread_exorcise();

    /* Activate our address space in the MMU. */
    // lcr3(PADDR(thisthread->page_directory));

    sti();

    while (random() & 0x3)
        thread_yield();

    int ret = entrypoint(data1, data2);

    cli();
    thread_exit(ret);

}

void
thread_exit(int ret) {
    assert(cli() == false);  // interrupts are off
    thisthread->rval = ret;

    if (thisthread->parent != NULL) {
        V(thisthread->psem);
        P(thisthread->csem);

        semaphore_destroy(thisthread->psem);
        semaphore_destroy(thisthread->csem);

        thisthread->psem = NULL;
        thisthread->csem = NULL;

        thisthread->parent = NULL;
    }

    proc_remthread(thisthread);
    assert(thisthread->proc == NULL);

    thread_checkstack(thisthread);

    assert(thiscpu->status != CPU_IDLE);

    thread_switch(S_ZOMBIE, NULL, NULL);

    panic("thread_switch returned\n");
}

void
thread_destroy(struct thread* thread) {
    assert(cli() == false);  // interrupts are off
    assert(thread != thisthread);
    assert(thread->state != S_RUN);
    assert(thread->proc == NULL);

    // if (thread->page_directory != NULL)
    //     kfree(thread->page_directory);      // not correct

    thread_returnstack(thread->stack);

    threadlistnode_cleanup(&thread->listnode);

    thread->wchan_name = "DESTROYED";

    kfree(thread->name);
    kfree(thread);
}

void
thread_exorcise(void) {
    assert(cli() == false);  // interrupts are off
    struct thread* thread;

    while ((thread = threadlist_remhead(&thiscpu->zombie_threads)) != NULL) {
        assert(thread != NULL);
        assert(thread != thisthread);
        assert(thread->state == S_ZOMBIE);
        thread_destroy(thread);
    }
}

int thread_join(struct thread* thread, int* ret_out) {
    assert(thread != NULL);
    assert(ret_out != NULL);
    assert(thread->parent != NULL);

    bool on = cli();

    if (thread == thisthread)
        return EDEADLK;

    assert(thread->parent == thisthread);

    P(thread->psem);
    V(thread->csem);

    *ret_out = thread->rval;
    thread->parent = NULL;

    ifx(on);

    return 0;
}

void thread_panic(void) {

    // // Kill off other CPUs.
    // // We could wait for them to stop, except that they might not.
    // ipi_broadcast(IPI_PANIC);

    /*
     * Drop runnable threads on the floor.
     *
     * Don't try to get the run queue lock; we might not be able
     * to.  Instead, blat the list structure by hand, and take the
     * risk that it might not be quite atomic.
     */
    thiscpu->active_threads.tl_count = 0;
    thiscpu->active_threads.tl_head.tln_next = &thiscpu->active_threads.tl_tail;
    thiscpu->active_threads.tl_tail.tln_prev = &thiscpu->active_threads.tl_head;
}

void thread_shutdown(void) {

}

void
thread_make_runnable(struct thread* thread, bool holding_lock) {
    bool on = cli();

    struct cpu* cpu = thread->cpu;

    thread->state = S_READY;

    if (holding_lock)
        assert(spinlock_held(&cpu->active_threads_lock));
    else
        spinlock_acquire(&cpu->active_threads_lock);

    threadlist_addtail(&cpu->active_threads, thread);

    /*
     * Other processor is idle; send interrupt to make
     * sure it unidles.
     */
    if (cpu->status == CPU_IDLE) {
        // ipi_send(cpu, IPI_UNIDLE);
        panic("CPU_IDLE");
    }

    if (!holding_lock)
        spinlock_release(&cpu->active_threads_lock);

    ifx(on);
}
