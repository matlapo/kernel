	.text
	.attribute	4, 16
	.attribute	5, "rv32i2p1_m2p0_a2p1_c2p0"
	.file	"relocatable.c"
	.globl	hello                           # -- Begin function hello
	.p2align	1
	.type	hello,@function
hello:                                  # @hello
# %bb.0:
	addi	sp, sp, -16
	sw	ra, 12(sp)                      # 4-byte Folded Spill
	sw	s0, 8(sp)                       # 4-byte Folded Spill
	addi	s0, sp, 16
	lui	a0, %hi(global_var)
	lw	a0, %lo(global_var)(a0)
	sw	a0, -12(s0)
	lw	a0, -12(s0)
	lw	ra, 12(sp)                      # 4-byte Folded Reload
	lw	s0, 8(sp)                       # 4-byte Folded Reload
	addi	sp, sp, 16
	ret
.Lfunc_end0:
	.size	hello, .Lfunc_end0-hello
                                        # -- End function
	.globl	main                            # -- Begin function main
	.p2align	1
	.type	main,@function
main:                                   # @main
# %bb.0:
	addi	sp, sp, -16
	sw	ra, 12(sp)                      # 4-byte Folded Spill
	sw	s0, 8(sp)                       # 4-byte Folded Spill
	addi	s0, sp, 16
	call	hello
	sw	a0, -12(s0)
	lw	a0, -12(s0)
	lw	ra, 12(sp)                      # 4-byte Folded Reload
	lw	s0, 8(sp)                       # 4-byte Folded Reload
	addi	sp, sp, 16
	ret
.Lfunc_end1:
	.size	main, .Lfunc_end1-main
                                        # -- End function
	.globl	_start                          # -- Begin function _start
	.p2align	1
	.type	_start,@function
_start:                                 # @_start
# %bb.0:
	addi	sp, sp, -16
	sw	ra, 12(sp)                      # 4-byte Folded Spill
	sw	s0, 8(sp)                       # 4-byte Folded Spill
	addi	s0, sp, 16
	call	main
	lw	ra, 12(sp)                      # 4-byte Folded Reload
	lw	s0, 8(sp)                       # 4-byte Folded Reload
	addi	sp, sp, 16
	ret
.Lfunc_end2:
	.size	_start, .Lfunc_end2-_start
                                        # -- End function
	.type	global_var,@object              # @global_var
	.section	.sdata,"aw",@progbits
	.globl	global_var
	.p2align	2, 0x0
global_var:
	.word	42                              # 0x2a
	.size	global_var, 4

	.ident	"clang version 18.1.8 (Fedora 18.1.8-1.fc40)"
	.section	".note.GNU-stack","",@progbits
	.addrsig
	.addrsig_sym hello
	.addrsig_sym main
	.addrsig_sym global_var
