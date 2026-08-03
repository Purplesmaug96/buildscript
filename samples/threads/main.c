// threads: threads, delays, and the system clock
//
// Spawns a couple of kernel threads with ExCreateThread, each incrementing
// its own counter, while main() periodically prints the state and the
// current kernel time (100ns units since 1601-01-01).

#include <stdio.h>

#include <xecore/xboxkrnl.h>

typedef struct worker_arg
{
    const char *name;
    volatile unsigned counter;
} worker_arg_t;

static worker_arg_t worker_a = {"thread A", 0};
static worker_arg_t worker_b = {"thread B", 0};

static void worker_thread(void *context)
{
    worker_arg_t *arg = (worker_arg_t *)context;

    for (;;)
    {
        arg->counter++;
        // Sleep for ~100 ms.
        int64_t interval = -100 * 1000 * 1000; // negative = relative, 100ns units
        KeDelayExecutionThread(0, 0, &interval);
    }
}

void main(void)
{
    HANDLE handle_a, handle_b;
    uint32_t thread_id_a, thread_id_b;

    NTSTATUS status_a = ExCreateThread(&handle_a, 0x10000, &thread_id_a,
                                       NULL, worker_thread, &worker_a, 0);
    NTSTATUS status_b = ExCreateThread(&handle_b, 0x10000, &thread_id_b,
                                       NULL, worker_thread, &worker_b, 0);

    printf("threads: ExCreateThread A -> 0x%08X (id %u)\n", status_a, thread_id_a);
    printf("threads: ExCreateThread B -> 0x%08X (id %u)\n", status_b, thread_id_b);

    for (int i = 0; i < 10; i++)
    {
        int64_t now = 0;
        KeQuerySystemTime(&now);
        unsigned long long secs = (unsigned long long)(now / 10000000);

        printf("t=%llus  %s=%u  %s=%u\n",
               secs, worker_a.name, worker_a.counter,
               worker_b.name, worker_b.counter);

        int64_t interval = -1000 * 1000 * 1000; // 1 s
        KeDelayExecutionThread(0, 0, &interval);
    }

    printf("threads: done, exiting to dashboard.\n");
}
