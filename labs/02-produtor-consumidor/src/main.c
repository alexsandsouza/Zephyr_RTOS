#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/time_units.h>
#include <stdint.h>
#include <stdbool.h>

#define JOBS 30
#define PERIOD_MS 20
#define DEADLINE_US 20000ULL
#define TEMP_LIMIT 70
#define END_JOB_ID 0U

struct leitura {
    uint32_t job_id;
    int32_t temperatura;
    uint64_t release_cycles;
};
struct comando {
    uint32_t job_id;
    bool alarme;
    uint64_t release_cycles;
};

K_MSGQ_DEFINE(fila_leituras, sizeof(struct leitura), 8, 4);
K_MSGQ_DEFINE(fila_comandos, sizeof(struct comando), 8, 4);

static uint32_t drop_leituras;
static uint32_t drop_comandos;
static uint32_t concluidos;
static uint32_t deadlines_perdidos;
static uint64_t pior_resposta_us;
static uint64_t maior_jitter_us;

/* Duracoes sao medidas em ciclos e convertidas para microssegundos. */
static uint64_t decorrido_us(uint64_t inicio, uint64_t fim)
{
    return k_cyc_to_us_floor64(fim - inicio);
}

static void produtor(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    int64_t base_ms = k_uptime_get();
    uint64_t base_cycles = k_cycle_get_64();

    for (uint32_t id = 1; id <= JOBS; ++id) {
        /* Proxima liberacao planejada, sem deriva cumulativa do k_sleep. */
        int64_t previsto_ms = base_ms + (int64_t)(id - 1) * PERIOD_MS;
        int64_t agora_ms = k_uptime_get();
        if (previsto_ms > agora_ms) {
            k_sleep(K_MSEC(previsto_ms - agora_ms));
        }

        uint64_t release_cycles = k_cycle_get_64();
        uint64_t real_us = decorrido_us(base_cycles, release_cycles);
        uint64_t previsto_us = (uint64_t)(id - 1) * PERIOD_MS * 1000ULL;
        uint64_t jitter_us = real_us >= previsto_us ?
                             real_us - previsto_us : previsto_us - real_us;
        if (jitter_us > maior_jitter_us) {
            maior_jitter_us = jitter_us;
        }

        struct leitura dado = {
            .job_id = id,
            .temperatura = 55 + (int32_t)((id * 7U) % 35U),
            .release_cycles = release_cycles
        };

        if (k_msgq_put(&fila_leituras, &dado, K_NO_WAIT) != 0) {
            ++drop_leituras;
            printk("DROP_LEITURA job=%u fila_cheia\n", id);
        } else {
            printk("RELEASE job=%u t_rel=%llu us jitter=%llu us valor=%d\n",
                   id, (unsigned long long)real_us,
                   (unsigned long long)jitter_us, dado.temperatura);
        }
    }

    /* Sentinela: permite encerrar o consumidor apos as mensagens restantes. */
    struct leitura fim = {.job_id = END_JOB_ID};
    k_msgq_put(&fila_leituras, &fim, K_FOREVER);
    printk("PRODUTOR_FINALIZADO jobs=%d drop_leituras=%u\n",
           JOBS, drop_leituras);
}

static void consumidor(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    struct leitura dado;
    while (true) {
        k_msgq_get(&fila_leituras, &dado, K_FOREVER);
        if (dado.job_id == END_JOB_ID) {
            struct comando fim = {.job_id = END_JOB_ID};
            k_msgq_put(&fila_comandos, &fim, K_FOREVER);
            printk("CONSUMIDOR_FINALIZADO drop_comandos=%u\n", drop_comandos);
            return;
        }

        struct comando cmd = {
            .job_id = dado.job_id,
            .alarme = dado.temperatura >= TEMP_LIMIT,
            .release_cycles = dado.release_cycles
        };

        if (k_msgq_put(&fila_comandos, &cmd, K_NO_WAIT) != 0) {
            ++drop_comandos;
            printk("DROP_COMANDO job=%u fila_cheia\n", dado.job_id);
        }
    }
}

static void atuador(void *a, void *b, void *c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);

    struct comando cmd;
    while (true) {
        k_msgq_get(&fila_comandos, &cmd, K_FOREVER);
        if (cmd.job_id == END_JOB_ID) {
            uint32_t contabilizados = concluidos + drop_leituras + drop_comandos;
            printk("\n=== RESUMO LAB 02 ===\n");
            printk("jobs_planejados=%d concluidos=%u\n", JOBS, concluidos);
            printk("drop_leituras=%u drop_comandos=%u\n",
                   drop_leituras, drop_comandos);
            printk("deadlines_perdidos=%u pior_resposta=%llu us\n",
                   deadlines_perdidos, (unsigned long long)pior_resposta_us);
            printk("maior_jitter_liberacao=%llu us\n",
                   (unsigned long long)maior_jitter_us);
            printk("contabilizados=%u consistencia=%s\n",
                   contabilizados, contabilizados == JOBS ? "OK" : "ERRO");
            printk("THREADS_FINALIZADAS - encerre o QEMU com Ctrl+C.\n");
            return;
        }

        /* Captura ANTES de imprimir; printk nao integra a medicao. */
        uint64_t fim_cycles = k_cycle_get_64();
        uint64_t resposta_us = decorrido_us(cmd.release_cycles, fim_cycles);
        bool prazo = resposta_us <= DEADLINE_US;
        ++concluidos;
        if (!prazo) {
            ++deadlines_perdidos;
        }
        if (resposta_us > pior_resposta_us) {
            pior_resposta_us = resposta_us;
        }

        printk("FINISH job=%u led=%s resposta=%llu us deadline=%llu us status=%s\n",
               cmd.job_id, cmd.alarme ? "ON" : "OFF",
               (unsigned long long)resposta_us,
               (unsigned long long)DEADLINE_US,
               prazo ? "OK" : "MISSED");
    }
}

K_THREAD_DEFINE(produtor_id, 2048, produtor, NULL, NULL, NULL, 4, 0, 0);
K_THREAD_DEFINE(consumidor_id, 2048, consumidor, NULL, NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(atuador_id, 2048, atuador, NULL, NULL, NULL, 2, 0, 0);

int main(void)
{
    printk("LAB 02 v2 - Produtor, consumidor e atuador no QEMU\n");
    printk("Jobs=%d periodo=%d ms deadline=%llu us\n",
           JOBS, PERIOD_MS, (unsigned long long)DEADLINE_US);
    printk("LED virtual, tempos experimentais (nao e WCET).\n");
    return 0;
}
