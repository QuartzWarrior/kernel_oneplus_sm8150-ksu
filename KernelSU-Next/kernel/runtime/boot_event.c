#include <linux/err.h>
#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/printk.h>
#include <linux/workqueue.h>

#include "policy/allowlist.h"
#include "klog.h" // IWYU pragma: keep
#include "runtime/ksud_boot.h"
#include "runtime/ksud.h"
#include "manager/manager_observer.h"
#include "manager/throne_tracker.h"

extern void apply_kernelsu_rules(void);
extern void cache_sid(void);
extern void setup_ksu_cred(void);

static void delayed_throne_work_fn(struct work_struct *work)
{
	track_throne(false);
}
static DECLARE_DELAYED_WORK(delayed_throne_work, delayed_throne_work_fn);

bool ksu_module_mounted __read_mostly = false;
bool ksu_boot_completed __read_mostly = false;
extern void stop_input_hook();

extern void ksu_avc_spoof_late_init();

void on_post_fs_data(void)
{
	static bool done = false;
	if (done) {
		pr_info("on_post_fs_data already done\n");
		return;
	}
	done = true;
	pr_info("on_post_fs_data!\n");

	apply_kernelsu_rules();
	cache_sid();
	setup_ksu_cred();
	ksu_load_allow_list();
	ksu_observer_init();
	track_throne(false);
	/* Retry after 3 s — packages.list may not be ready yet when zygote starts */
	schedule_delayed_work(&delayed_throne_work, msecs_to_jiffies(3000));
	// sanity check, this may influence the performance
	stop_input_hook();
}

extern void ext4_unregister_sysfs(struct super_block *sb);

int nuke_ext4_sysfs(const char *mnt)
{
	struct path path;
	int err = kern_path(mnt, 0, &path);
	if (err) {
		pr_err("nuke path err: %d\n", err);
		return err;
	}

	struct super_block *sb = path.dentry->d_inode->i_sb;
	const char *name = sb->s_type->name;
	if (strcmp(name, "ext4") != 0) {
		pr_info("nuke but module aren't mounted\n");
		path_put(&path);
		return -EINVAL;
	}

	ext4_unregister_sysfs(sb);
	path_put(&path);
	return 0;
}

void on_module_mounted(void)
{
	pr_info("on_module_mounted!\n");
	ksu_module_mounted = true;
}

void on_boot_completed(void)
{
    ksu_boot_completed = true;
    pr_info("on_boot_completed!\n");
    track_throne(true);
    ksu_avc_spoof_late_init();
}
