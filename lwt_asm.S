	.text

	.align 16
.globl __lwt_trampoline
	.type	__lwt_trampoline, @function
__lwt_trampoline:
	# In create, two parameters for start were added.
	# Should be consistent with ebp
	addl $8, %ebp
	call __lwt_start

	.align 16
.globl __lwt_dispatch
	.type	__lwt_dispatch, @function
__lwt_dispatch:
	# Parameters: edx is next and ecx is current
	# should point to structs
	movl	4(%esp), %edx
	movl	8(%esp), %ecx

	# save registers
	pushal
	pushfl

	# old->esp = esp
	movl	%esp, 4(%ecx)
	# esp = next->esp
	movl	4(%edx), %esp
	# old->ebp = ebp
	movl	%ebp, 8(%ecx)
	# ebp = next->ebp
	movl	8(%edx), %ebp
	# old->epi = $__lwt_capture
	movl	$__lwt_capture, (%ecx)
	# variable to jump to = next->eip
	movl	(%edx), %ecx
	# context switch
	jmp	*%ecx
__lwt_capture:
	# Restore registers
	popfl
	popal
	ret
