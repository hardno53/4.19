#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/asm-uaccess.h>

ENTRY(__flush_dcache_user_area)
	uaccess_ttbr0_enable x2, x3, x4
	dcache_by_line_op civac, sy, x0, x1, x2, x3
	uaccess_ttbr0_disable x1, x2
	ret
ENDPROC(__flush_dcache_user_area)

ENTRY(__clean_dcache_user_area)
	uaccess_ttbr0_enable x2, x3, x4
	dcache_by_line_op cvac, sy, x0, x1, x2, x3
	uaccess_ttbr0_disable x1, x2
	ret
ENDPROC(__clean_dcache_user_area)

ENTRY(__inval_dcache_user_area)
	add     x1, x1, x0
	uaccess_ttbr0_enable x2, x3, x4
	dcache_line_size x2, x3
	sub     x3, x2, #1
	tst     x1, x3                          // end cache line aligned?
	bic     x1, x1, x3
	b.eq    1f
	dc      civac, x1                       // clean & invalidate D / U line
1:      tst     x0, x3                          // start cache line aligned?
	bic     x0, x0, x3
	b.eq    2f
	dc      civac, x0                       // clean & invalidate D / U line
	b       3f
2:      dc      civac, x0                        // invalidate D / U line
3:      add     x0, x0, x2
	cmp     x0, x1
	b.lo    2b
	dsb     sy
	uaccess_ttbr0_disable x1, x2
	ret
ENDPROC(__inval_dcache_user_area)
