#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/*
 * 0: thread A preemptiva com prioridade 4.
 * 1: thread A cooperativa com prioridade -1.
 * B usa prioridade preemptiva 2 em ambos os casos.
 */
#define SCENARIO_COOPERATIVE 0

#if SCENARIO_COOPERATIVE
#define PRIORITY_A K_PRIO_COOP(1)
#else
#define PRIORITY_A K_PRIO_PREEMPT(4)
#endif
#define PRIORITY_B K_PRIO_PREEMPT(2)

K_SEM_DEFINE(start_a, 0, 1);

static void thread_a(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    k_sem_take(&start_a, K_FOREVER);
    printk("A START t=%lld ms\n", (long long)k_uptime_get());
    k_busy_wait(10000);
    printk("A FINISH t=%lld ms\n", (long long)k_uptime_get());
}

static void thread_b(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    k_sleep(K_MSEC(2));
    printk("B START t=%lld ms\n", (long long)k_uptime_get());
    k_busy_wait(2000);
    printk("B FINISH t=%lld ms\n", (long long)k_uptime_get());
}

K_THREAD_DEFINE(a_id, 2048, thread_a, NULL, NULL, NULL, PRIORITY_A, 0, 0);
K_THREAD_DEFINE(b_id, 2048, thread_b, NULL, NULL, NULL, PRIORITY_B, 0, 0);

int main(void)
{
    printk("LAB 03 - SCENARIO_COOPERATIVE=%d\n", SCENARIO_COOPERATIVE);
    k_sem_give(&start_a);
    return 0;
}
