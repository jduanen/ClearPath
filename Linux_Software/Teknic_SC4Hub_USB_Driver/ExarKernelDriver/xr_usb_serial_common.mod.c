#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};



static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x8e5094c8, "tty_termios_baud_rate" },
	{ 0x092a35a2, "_copy_from_user" },
	{ 0x0c161ddc, "capable" },
	{ 0x092a35a2, "_copy_to_user" },
	{ 0x7851be11, "__get_user_4" },
	{ 0xd272d446, "__put_user_4" },
	{ 0x86891b00, "__tty_insert_flip_string_flags" },
	{ 0x894786aa, "tty_flip_buffer_push" },
	{ 0xe67fde35, "tty_standard_install" },
	{ 0xff0106da, "refcount_warn_saturate" },
	{ 0xaef1f20d, "system_percpu_wq" },
	{ 0x49733ad6, "queue_work_on" },
	{ 0xad5531ee, "usb_put_intf" },
	{ 0xe42d9d19, "gpiochip_remove" },
	{ 0xaf0ec592, "usb_ifnum_to_if" },
	{ 0x9aa6980d, "mutex_init_generic" },
	{ 0x92c5f7d6, "tty_port_init" },
	{ 0xb9003bb3, "usb_alloc_coherent" },
	{ 0x773eb86a, "usb_alloc_urb" },
	{ 0xdf9cca1e, "device_create_file" },
	{ 0xd710adbf, "__kmalloc_noprof" },
	{ 0x529b94b4, "device_remove_file" },
	{ 0x7d401011, "usb_free_coherent" },
	{ 0x88b75d86, "_dev_warn" },
	{ 0x251d9cb7, "usb_free_urb" },
	{ 0xe54e0a6b, "__fortify_panic" },
	{ 0x88b75d86, "_dev_info" },
	{ 0x088616a6, "usb_driver_claim_interface" },
	{ 0x3527dd0e, "usb_get_intf" },
	{ 0x9a46f760, "tty_port_register_device" },
	{ 0x6e9b719f, "gpiochip_add_data_with_key" },
	{ 0x746eb154, "tty_port_tty_get" },
	{ 0x90813702, "tty_vhangup" },
	{ 0x90813702, "tty_kref_put" },
	{ 0xaa5303d9, "tty_unregister_device" },
	{ 0x3fdde6b2, "usb_driver_release_interface" },
	{ 0xd272d446, "__fentry__" },
	{ 0x5af09d8b, "_raw_spin_lock_irq" },
	{ 0x5af09d8b, "_raw_spin_unlock_irq" },
	{ 0xd272d446, "__x86_return_thunk" },
	{ 0x7a4a235c, "__tty_alloc_driver" },
	{ 0xf4fa66aa, "tty_std_termios" },
	{ 0x99b8bcd2, "tty_register_driver" },
	{ 0x3fc0ae15, "usb_register_driver" },
	{ 0x9ff29899, "tty_unregister_driver" },
	{ 0x9ff29899, "tty_driver_kref_put" },
	{ 0xe8213e80, "_printk" },
	{ 0x0b0a7e9f, "usb_submit_urb" },
	{ 0x88b75d86, "_dev_err" },
	{ 0xfb8382ea, "usb_autopm_put_interface_async" },
	{ 0x251d9cb7, "usb_kill_urb" },
	{ 0x2d88a3ab, "cancel_work_sync" },
	{ 0xa96712e6, "__dynamic_dev_dbg" },
	{ 0x90a48d82, "__ubsan_handle_out_of_bounds" },
	{ 0xd272d446, "dump_stack" },
	{ 0x5af09d8b, "_raw_spin_lock" },
	{ 0x5af09d8b, "_raw_spin_unlock" },
	{ 0x92c5f7d6, "tty_port_put" },
	{ 0xdd6830c7, "sprintf" },
	{ 0xfbe7861b, "memcpy" },
	{ 0x0693e457, "usb_control_msg" },
	{ 0x82fd7238, "__ubsan_handle_shift_out_of_bounds" },
	{ 0xa6c3b74d, "ktime_get_mono_fast_ns" },
	{ 0x08a509e1, "__tty_port_tty_hangup" },
	{ 0x92c5f7d6, "tty_port_tty_wakeup" },
	{ 0xbd03ed67, "random_kmalloc_seed" },
	{ 0xa65b83f9, "kmalloc_caches" },
	{ 0xce56ea17, "__kmalloc_cache_noprof" },
	{ 0xcb8b6ec6, "kfree" },
	{ 0xbd03ed67, "__ref_stack_chk_guard" },
	{ 0xd272d446, "__stack_chk_fail" },
	{ 0x92c5f7d6, "tty_port_hangup" },
	{ 0x11f4259a, "_raw_spin_lock_irqsave" },
	{ 0x444885a7, "_raw_spin_unlock_irqrestore" },
	{ 0x07448b9c, "usb_autopm_get_interface_async" },
	{ 0x7d8e36f3, "tty_port_close" },
	{ 0x645ed827, "usb_deregister" },
	{ 0xe4de56b4, "__ubsan_handle_load_invalid_value" },
	{ 0x9aa6980d, "mutex_lock" },
	{ 0x9aa6980d, "mutex_unlock" },
	{ 0x07448b9c, "usb_autopm_get_interface" },
	{ 0xfb8382ea, "usb_autopm_put_interface" },
	{ 0x151cfdba, "tty_port_open" },
	{ 0xe9196a28, "module_layout" },
};

static const u32 ____version_ext_crcs[]
__used __section("__version_ext_crcs") = {
	0x8e5094c8,
	0x092a35a2,
	0x0c161ddc,
	0x092a35a2,
	0x7851be11,
	0xd272d446,
	0x86891b00,
	0x894786aa,
	0xe67fde35,
	0xff0106da,
	0xaef1f20d,
	0x49733ad6,
	0xad5531ee,
	0xe42d9d19,
	0xaf0ec592,
	0x9aa6980d,
	0x92c5f7d6,
	0xb9003bb3,
	0x773eb86a,
	0xdf9cca1e,
	0xd710adbf,
	0x529b94b4,
	0x7d401011,
	0x88b75d86,
	0x251d9cb7,
	0xe54e0a6b,
	0x88b75d86,
	0x088616a6,
	0x3527dd0e,
	0x9a46f760,
	0x6e9b719f,
	0x746eb154,
	0x90813702,
	0x90813702,
	0xaa5303d9,
	0x3fdde6b2,
	0xd272d446,
	0x5af09d8b,
	0x5af09d8b,
	0xd272d446,
	0x7a4a235c,
	0xf4fa66aa,
	0x99b8bcd2,
	0x3fc0ae15,
	0x9ff29899,
	0x9ff29899,
	0xe8213e80,
	0x0b0a7e9f,
	0x88b75d86,
	0xfb8382ea,
	0x251d9cb7,
	0x2d88a3ab,
	0xa96712e6,
	0x90a48d82,
	0xd272d446,
	0x5af09d8b,
	0x5af09d8b,
	0x92c5f7d6,
	0xdd6830c7,
	0xfbe7861b,
	0x0693e457,
	0x82fd7238,
	0xa6c3b74d,
	0x08a509e1,
	0x92c5f7d6,
	0xbd03ed67,
	0xa65b83f9,
	0xce56ea17,
	0xcb8b6ec6,
	0xbd03ed67,
	0xd272d446,
	0x92c5f7d6,
	0x11f4259a,
	0x444885a7,
	0x07448b9c,
	0x7d8e36f3,
	0x645ed827,
	0xe4de56b4,
	0x9aa6980d,
	0x9aa6980d,
	0x07448b9c,
	0xfb8382ea,
	0x151cfdba,
	0xe9196a28,
};
static const char ____version_ext_names[]
__used __section("__version_ext_names") =
	"tty_termios_baud_rate\0"
	"_copy_from_user\0"
	"capable\0"
	"_copy_to_user\0"
	"__get_user_4\0"
	"__put_user_4\0"
	"__tty_insert_flip_string_flags\0"
	"tty_flip_buffer_push\0"
	"tty_standard_install\0"
	"refcount_warn_saturate\0"
	"system_percpu_wq\0"
	"queue_work_on\0"
	"usb_put_intf\0"
	"gpiochip_remove\0"
	"usb_ifnum_to_if\0"
	"mutex_init_generic\0"
	"tty_port_init\0"
	"usb_alloc_coherent\0"
	"usb_alloc_urb\0"
	"device_create_file\0"
	"__kmalloc_noprof\0"
	"device_remove_file\0"
	"usb_free_coherent\0"
	"_dev_warn\0"
	"usb_free_urb\0"
	"__fortify_panic\0"
	"_dev_info\0"
	"usb_driver_claim_interface\0"
	"usb_get_intf\0"
	"tty_port_register_device\0"
	"gpiochip_add_data_with_key\0"
	"tty_port_tty_get\0"
	"tty_vhangup\0"
	"tty_kref_put\0"
	"tty_unregister_device\0"
	"usb_driver_release_interface\0"
	"__fentry__\0"
	"_raw_spin_lock_irq\0"
	"_raw_spin_unlock_irq\0"
	"__x86_return_thunk\0"
	"__tty_alloc_driver\0"
	"tty_std_termios\0"
	"tty_register_driver\0"
	"usb_register_driver\0"
	"tty_unregister_driver\0"
	"tty_driver_kref_put\0"
	"_printk\0"
	"usb_submit_urb\0"
	"_dev_err\0"
	"usb_autopm_put_interface_async\0"
	"usb_kill_urb\0"
	"cancel_work_sync\0"
	"__dynamic_dev_dbg\0"
	"__ubsan_handle_out_of_bounds\0"
	"dump_stack\0"
	"_raw_spin_lock\0"
	"_raw_spin_unlock\0"
	"tty_port_put\0"
	"sprintf\0"
	"memcpy\0"
	"usb_control_msg\0"
	"__ubsan_handle_shift_out_of_bounds\0"
	"ktime_get_mono_fast_ns\0"
	"__tty_port_tty_hangup\0"
	"tty_port_tty_wakeup\0"
	"random_kmalloc_seed\0"
	"kmalloc_caches\0"
	"__kmalloc_cache_noprof\0"
	"kfree\0"
	"__ref_stack_chk_guard\0"
	"__stack_chk_fail\0"
	"tty_port_hangup\0"
	"_raw_spin_lock_irqsave\0"
	"_raw_spin_unlock_irqrestore\0"
	"usb_autopm_get_interface_async\0"
	"tty_port_close\0"
	"usb_deregister\0"
	"__ubsan_handle_load_invalid_value\0"
	"mutex_lock\0"
	"mutex_unlock\0"
	"usb_autopm_get_interface\0"
	"usb_autopm_put_interface\0"
	"tty_port_open\0"
	"module_layout\0"
;

MODULE_INFO(depends, "");

MODULE_ALIAS("usb:v04E2p1410d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v04E2p1411d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v04E2p1412d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v04E2p1414d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v04E2p1420d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v04E2p1421d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v04E2p1422d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v04E2p1424d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v04E2p1400d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v04E2p1401d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v04E2p1402d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v04E2p1403d*dc*dsc*dp*ic*isc*ip*in*");
MODULE_ALIAS("usb:v2890p0213d*dc*dsc*dp*ic*isc*ip*in*");

MODULE_INFO(srcversion, "46BC211BC3F7AA5FE8FDBAE");
