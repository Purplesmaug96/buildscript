/* setjmp/longjmp for the console (ppc32, ELF-style function ABI).
 *
 * Newlib does not build a machine/setjmp for PowerPC, but the Mesa/NIR
 * objects call setjmp() (SPIR-V error unwinding and u_cpu_detect), so the
 * shim provides a function-call implementation.  jmp_buf is long enough
 * (64 doubles on __PPC__) for our 32-slot layout:
 *
 *   [ 0]  r1  (stack pointer)
 *   [ 1]  lr
 *   [ 2..20]  r13 .. r31   (callee-saved GPRs)
 *
 * This matches the GCC/Clang convention for the function-call version of
 * setjmp on powerpc32; clang emits plain calls to these symbols for
 * setjmp()/longjmp() (no __builtin_setjmp conversion), so the layout is
 * private to this pair.
 */

	.text
	.globl	setjmp
setjmp:
	stw	1, 0(3)		/* r1  -> [0]  */
	mflr	12
	stw	12, 4(3)	/* lr  -> [1]  */
	stmw	13, 8(3)	/* r13-r31 -> [2..20] */
	li	3, 0		/* first return is 0 */
	blr

	.globl	longjmp
longjmp:
	cmpwi	4, 0		/* value of 0 becomes 1 */
	bne	1f
	li	4, 1
1:
	lmw	13, 8(3)	/* r13-r31 <- [2..20] */
	lwz	12, 4(3)
	mtlr	12
	lwz	1, 0(3)		/* restore stack pointer  */
	mr	3, 4		/* return value          */
	blr

