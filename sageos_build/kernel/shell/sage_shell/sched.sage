# =============================================================================
# SageOS Scheduler Display — Pure SageLang Port
# sched.sage
#
# C source kept: sageos_build/kernel/core/scheduler.c (UNCHANGED — bare metal)
#
# scheduler.c contains naked __asm__ context switching, GCC atomic spinlocks,
# inline IRQ save/restore (pushfq/cli), and raw stack pointer arithmetic.
# None of these can be expressed in SageLang. The scheduler stays in C.
#
# This module ports the *display interface* only: sched_cmd_info() equivalent
# using the os_sched_* VM bindings into sched_get_stats().
# =============================================================================

proc cmd_sched():
    os_dmesg_log("sched: display requested from SageLang")

    os_write_str("\nScheduler:")
    os_write_str("\n  policy: weighted fair queues, cooperative switch, timer accounting")

    os_write_str("\n  cpus:    ")
    os_write_str(os_num_to_str(os_sched_cpu_count()))

    os_write_str("\n  threads: ")
    os_write_str(os_num_to_str(os_sched_thread_count()))

    os_write_str("\n  context switches: ")
    os_write_str(os_num_to_str(os_sched_context_switches()))

    os_write_str("\n  migrations:       ")
    os_write_str(os_num_to_str(os_sched_migrations()))

    os_write_str("\n")

    # Full per-CPU + thread list via C (raw thread_t walk needs pointer arithmetic)
    os_sched_info()
end
