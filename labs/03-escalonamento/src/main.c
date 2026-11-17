#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/time_units.h>
#include <stdint.h>

/*
 * Experimento no QEMU (CPU unica):
 * 0 = Thread A preemptiva, prioridade 4
 * 1 = Thread A cooperativa, prioridade -1
 *
 * Thread B tem prioridade preemptiva 2 em ambos os cenarios.
 * O timer libera B enquanto A executa um trecho sem yield/sleep.
 */
#define SCENARIO_COOPERATIVE 1

#define B_RELEASE_DELAY_MS 20
#define A_WORK_US 100000
#define B_WORK_US 2000
#define B_DEADLINE_US 30000ULL

#if SCENARIO_COOPERATIVE
#define PRIORITY_A K_PRIO_COOP(1)
#else
#define PRIORITY_A K_PRIO_PREEMPT(4)
#endif
#define PRIORITY_B K_PRIO_PREEMPT(2)

K_SEM_DEFINE(sem_b, 0, 1);
K_SEM_DEFINE(done_a, 0, 1);
K_SEM_DEFINE(done_b, 0, 1);

static uint64_t reference_cycles;
static uint64_t b_release_cycles;
static uint64_t a_start_cycles;
static uint64_t a_finish_cycles;
static uint64_t b_start_cycles;
static uint64_t b_finish_cycles;

static uint64_t rel_us(uint64_t cycles)
{
    return k_cyc_to_us_floor64(cycles - reference_cycles);
}

static void release_b(struct k_timer *timer)
{
    ARG_UNUSED(timer);
    /* Contexto de interrupcao: capturar primeiro e acordar B depois. */
    b_release_cycles = k_cycle_get_64();
    k_sem_give(&sem_b);
}

K_TIMER_DEFINE(release_timer, release_b, NULL);

static void thread_a(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    a_start_cycles = k_cycle_get_64();
    reference_cycles = a_start_cycles;
    printk("A START t=0 us\n");

    /* Timer e ISR independem de A ceder a CPU. */
    k_timer_start(&release_timer, K_MSEC(B_RELEASE_DELAY_MS), K_NO_WAIT);
    k_busy_wait(A_WORK_US);
    a_finish_cycles = k_cycle_get_64();

    printk("A FINISH t=%llu us\n",
           (unsigned long long)rel_us(a_finish_cycles));
    k_sem_give(&done_a);
}

static void thread_b(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    k_sem_take(&sem_b, K_FOREVER);
    b_start_cycles = k_cycle_get_64();

    /* Captura dos eventos ANTES do printk, reduzindo interferencia dos logs. */
    uint64_t espera_us = k_cyc_to_us_floor64(b_start_cycles - b_release_cycles);
    printk("B RELEASE t=%llu us (timer ISR)\n",
           (unsigned long long)rel_us(b_release_cycles));
    printk("B START t=%llu us espera=%llu us\n",
           (unsigned long long)rel_us(b_start_cycles),
           (unsigned long long)espera_us);

    k_busy_wait(B_WORK_US);
    b_finish_cycles = k_cycle_get_64();
    printk("B FINISH t=%llu us\n",
           (unsigned long long)rel_us(b_finish_cycles));
    k_sem_give(&done_b);
}

K_THREAD_DEFINE(a_id, 2048, thread_a, NULL, NULL, NULL, PRIORITY_A, 0, 0);
K_THREAD_DEFINE(b_id, 2048, thread_b, NULL, NULL, NULL, PRIORITY_B, 0, 0);

int main(void)
{
    printk("LAB 03 v2 - SCENARIO_COOPERATIVE=%d\n",
           SCENARIO_COOPERATIVE);
    printk("Timer libera B apos %d ms de A; prazo relativo de B=%llu us\n",
           B_RELEASE_DELAY_MS, (unsigned long long)B_DEADLINE_US);

    k_sem_take(&done_a, K_FOREVER);
    k_sem_take(&done_b, K_FOREVER);

    uint64_t response_us = k_cyc_to_us_floor64(
        b_finish_cycles - b_release_cycles);
    bool preempted = (b_start_cycles < a_finish_cycles);
    printk("\n=== RESUMO LAB 03 ===\n");
    printk("cenario=%s\n",
           SCENARIO_COOPERATIVE ? "COOPERATIVO" : "PREEMPTIVO");
    printk("A_inicio=%llu A_fim=%llu us\n",
           (unsigned long long)rel_us(a_start_cycles),
           (unsigned long long)rel_us(a_finish_cycles));
    printk("B_release=%llu B_inicio=%llu B_fim=%llu us\n",
           (unsigned long long)rel_us(b_release_cycles),
           (unsigned long long)rel_us(b_start_cycles),
           (unsigned long long)rel_us(b_finish_cycles));
    printk("B_resposta=%llu us deadline=%llu us status=%s\n",
           (unsigned long long)response_us,
           (unsigned long long)B_DEADLINE_US,
           response_us <= B_DEADLINE_US ? "OK" : "MISSED");
    printk("B_iniciou_antes_de_A_terminar=%s\n",
           preempted ? "SIM" : "NAO");
    printk("Fim do experimento; Ctrl+C encerra o QEMU.\n");
    return 0;
}
