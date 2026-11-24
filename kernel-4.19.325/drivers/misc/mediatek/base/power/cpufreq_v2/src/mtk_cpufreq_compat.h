#ifndef _MTK_CPUFREQ_COMPAT_H
#define _MTK_CPUFREQ_COMPAT_H
#include <linux/cpumask.h>
#ifndef CPU_TASKS_FROZEN
#define CPU_TASKS_FROZEN 0x0010
#endif
static inline int arch_get_nr_clusters(void) { return 1; }
static inline int arch_get_cluster_id(unsigned int cpu) { return 0; }
static inline void arch_get_cluster_cpus(struct cpumask *cpus, int id)
{
	if (id == 0)
		cpumask_copy(cpus, cpu_possible_mask);
	else
		cpumask_clear(cpus);
}
#endif
