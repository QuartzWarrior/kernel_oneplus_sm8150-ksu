#ifndef KSU_SUSFS_COMPAT_H
#define KSU_SUSFS_COMPAT_H
/*
 * Bridge header included only by SukiSU-Ultra dispatch.c.
 * dispatch.c passes void __user ** but v1.5.5 susfs uses typed struct pointers.
 * These static inlines perform the cast and stub out APIs not in v1.5.5.
 */
#include <linux/susfs.h>

static inline void ksu_susfs_add_sus_path(void __user **p)
{
#ifdef CONFIG_KSU_SUSFS_SUS_PATH
	susfs_add_sus_path((struct st_susfs_sus_path __user *)*p);
#endif
}
static inline void ksu_susfs_add_sus_kstat(void __user **p)
{
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
	susfs_add_sus_kstat((struct st_susfs_sus_kstat __user *)*p);
#endif
}
static inline void ksu_susfs_update_sus_kstat(void __user **p)
{
#ifdef CONFIG_KSU_SUSFS_SUS_KSTAT
	susfs_update_sus_kstat((struct st_susfs_sus_kstat __user *)*p);
#endif
}
static inline void ksu_susfs_set_uname(void __user **p)
{
#ifdef CONFIG_KSU_SUSFS_SPOOF_UNAME
	susfs_set_uname((struct st_susfs_uname __user *)*p);
#endif
}
static inline void ksu_susfs_set_cmdline(void __user **p)
{
#ifdef CONFIG_KSU_SUSFS_SPOOF_CMDLINE_OR_BOOTCONFIG
	susfs_set_cmdline_or_bootconfig((char __user *)*p);
#endif
}
static inline void ksu_susfs_add_open_redirect(void __user **p)
{
#ifdef CONFIG_KSU_SUSFS_OPEN_REDIRECT
	susfs_add_open_redirect((struct st_susfs_open_redirect __user *)*p);
#endif
}

/* Map dispatch.c calls to our typed wrappers */
#define susfs_add_sus_path(p)                    ksu_susfs_add_sus_path(p)
#define susfs_add_sus_path_loop(p)               do {} while(0)
#define susfs_set_hide_sus_mnts_for_non_su_procs(p) do {} while(0)
#define susfs_add_sus_kstat(p)                   ksu_susfs_add_sus_kstat(p)
#define susfs_update_sus_kstat(p)                ksu_susfs_update_sus_kstat(p)
#define susfs_set_uname(p)                       ksu_susfs_set_uname(p)
#define susfs_enable_log(p)                      susfs_set_log(true)
#define susfs_set_cmdline_or_bootconfig(p)       ksu_susfs_set_cmdline(p)
#define susfs_add_open_redirect(p)               ksu_susfs_add_open_redirect(p)
#define susfs_add_sus_map(p)                     do {} while(0)
#define susfs_set_avc_log_spoofing(p)            do {} while(0)
#define susfs_get_enabled_features(p)            do {} while(0)
#define susfs_show_variant(p)                    do {} while(0)
#define susfs_show_version(p)                    do {} while(0)
#define susfs_start_sdcard_monitor_fn()          do {} while(0)

#endif /* KSU_SUSFS_COMPAT_H */
