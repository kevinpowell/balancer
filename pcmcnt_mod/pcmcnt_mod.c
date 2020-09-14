#include <linux/module.h>
#include <linux/kernel.h>

static void enable_ccnt_read(void* data)
{
  // PMUSERENR = 1
  asm volatile ("mcr p15, 0, %0, c9, c14, 0" :: "r"(1));

  // PMCR.E (bit 0) = 1
  asm volatile ("mcr p15, 0, %0, c9, c12, 0" :: "r"(1));

  // PMCNTENSET.C (bit 31) = 1
  asm volatile ("mcr p15, 0, %0, c9, c12, 1" :: "r"(1 << 31));
}

int init_module()
{
  on_each_cpu(enable_ccnt_read, NULL, 1);
  return 0;
}

void cleanup_module()
{
}

MODULE_LICENSE("GPL");

