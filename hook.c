#include "hook.h"

// Credit to: Filip Pynckels - MIT/GPL dual (http://users.telenet.be/pynckels/2020-2-Linux-kernel-unexported-syms-functions.pdf)
unsigned long lookup_name( const char *name ){
    struct kprobe kp;
    unsigned long retval;

    kp.symbol_name = name;
    if (register_kprobe(&kp) < 0) return 0;
    retval = (unsigned long)kp.addr;
    unregister_kprobe(&kp);
    return retval;
}

int start_hook(fthook_t *hook, uintptr_t hooked_function, ftrace_fn hook_function)
{
	int ret = 0;

	hook->active = 0;
	hook->original_function = hooked_function;
	hook->ops.func = hook_function;
	hook->ops.flags = FTRACE_OPS_FL_SAVE_REGS
		| FTRACE_OPS_FL_RECURSION_SAFE
		| FTRACE_OPS_FL_IPMODIFY;

	ret = ftrace_set_filter_ip(&hook->ops, hook->original_function, 0, 0);

	if (ret) {
		return 1;
	}

	ret = register_ftrace_function(&hook->ops);

	if (ret) {
		ftrace_set_filter_ip(&hook->ops, hook->original_function, 1, 0);
		return 2;
	}

	hook->active = 1;

	return 0;
}

int end_hook(fthook_t *hook)
{
	int ret = unregister_ftrace_function(&hook->ops);

	hook->active = 0;

	if (ret)
		return ret;

	ftrace_set_filter_ip(&hook->ops, hook->original_function, 1, 0);

	return 0;
}

int start_hook_list(const fthinit_t *hook_list, size_t size)
{
	size_t i;
	uintptr_t symaddr;
	int ret = 0;

	for (i = 0; i < size; i++) {
		if (hook_list[i].symbol_name) {
			symaddr = lookup_name(hook_list[i].symbol_name);
		} else {
			symaddr = hook_list[i].symbol_getter();
		}

		if (!symaddr) {
			continue;
		}

		ret = start_hook(hook_list[i].hook, symaddr, hook_list[i].hook_function);

		if (ret) {
			end_hook_list(hook_list, i);
			return ret;
		}
	}

	return 0;
}

int end_hook_list(const fthinit_t *hook_list, size_t size)
{
	size_t i;
	int ret = 0, ret2 = 0;

	for (i = 0; i < size; i++) {
		if (hook_list[i].hook->active)
			ret2 = end_hook(hook_list[i].hook);

		if (ret2)
			ret = ret2;
	}

	return ret;
}
