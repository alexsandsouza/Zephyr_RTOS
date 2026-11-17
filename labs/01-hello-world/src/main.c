#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

int main(void)
{
    printk("LAB 01 - Hello World! Zephyr RTOS no QEMU\n");
    printk("Kernel ativo; uptime: %lld ms\n", (long long)k_uptime_get());
    return 0;
}
