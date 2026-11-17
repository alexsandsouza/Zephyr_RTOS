#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>
#include <stdbool.h>

#define JOBS 30
#define PERIOD_MS 20
#define DEADLINE_MS 20
#define TEMP_LIMIT 70

struct leitura {
    uint32_t job_id;
    int32_t temperatura;
    int64_t release_ms;
};

struct comando {
    uint32_t job_id;
    bool alarme;
    int64_t release_ms;
};

K_MSGQ_DEFINE(fila_leituras, sizeof(struct leitura), 8, 4);
K_MSGQ_DEFINE(fila_comandos, sizeof(struct comando), 8, 4);

static void produtor(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    for (uint32_t id = 1; id <= JOBS; ++id) {
        /* Registro no instante em que o job realmente entra na funcao. */
        struct leitura dado = {
            .job_id = id,
            .temperatura = 55 + (int32_t)((id * 7) % 35),
            .release_ms = k_uptime_get()
        };

        if (k_msgq_put(&fila_leituras, &dado, K_NO_WAIT) != 0) {
            printk("DROP_LEITURA job=%u fila_cheia\n", id);
        } else {
            printk("RELEASE job=%u t=%lld valor=%d\n", id,
                   (long long)dado.release_ms, dado.temperatura);
        }
        k_sleep(K_MSEC(PERIOD_MS));
    }
    printk("PRODUTOR_FINALIZADO total=%d\n", JOBS);
}

static void consumidor(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    struct leitura dado;
    while (true) {
        k_msgq_get(&fila_leituras, &dado, K_FOREVER);
        struct comando cmd = {
            .job_id = dado.job_id,
            .alarme = dado.temperatura >= TEMP_LIMIT,
            .release_ms = dado.release_ms
        };
        if (k_msgq_put(&fila_comandos, &cmd, K_NO_WAIT) != 0) {
            printk("DROP_COMANDO job=%u fila_cheia\n", dado.job_id);
        }
    }
}

static void atuador(void *a, void *b, void *c)
{
    ARG_UNUSED(a); ARG_UNUSED(b); ARG_UNUSED(c);
    struct comando cmd;
    while (true) {
        k_msgq_get(&fila_comandos, &cmd, K_FOREVER);
        int64_t fim = k_uptime_get();
        int64_t resposta = fim - cmd.release_ms;
        bool prazo = resposta <= DEADLINE_MS;
        /* No QEMU nao ha LED fisico; a atuacao e um estado logico. */
        printk("FINISH job=%u led=%s fim=%lld resposta=%lldms deadline=%dms status=%s\n",
               cmd.job_id, cmd.alarme ? "ON" : "OFF",
               (long long)fim, (long long)resposta, DEADLINE_MS,
               prazo ? "OK" : "MISSED");
    }
}

K_THREAD_DEFINE(produtor_id, 2048, produtor, NULL, NULL, NULL, 4, 0, 0);
K_THREAD_DEFINE(consumidor_id, 2048, consumidor, NULL, NULL, NULL, 3, 0, 0);
K_THREAD_DEFINE(atuador_id, 2048, atuador, NULL, NULL, NULL, 2, 0, 0);

int main(void)
{
    printk("LAB 02 - Cadeia produtor -> consumidor -> atuador\n");
    printk("QEMU: LED virtual; periodicidade aproximada; timestamps em ms\n");
    return 0;
}
