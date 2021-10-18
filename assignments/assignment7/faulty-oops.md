# Oops Message from doing: echo “hello_world” > /dev/faulty
```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x96000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000042033000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 96000045 [#1] SMP
Modules linked in: hello(O) faulty(O) scull(O)
CPU: 0 PID: 153 Comm: sh Tainted: G           O      5.10.7 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO BTYPE=--)
pc : faulty_write+0x14/0x20 [faulty]
lr : vfs_write+0xf0/0x290
sp : ffffffc010ccbdc0
x29: ffffffc010ccbdc0 x28: ffffff8002005940 
x27: 0000000000000000 x26: 0000000000000000 
x25: 0000000000000000 x24: 0000000000000000 
x23: 0000000000000000 x22: ffffffc010ccbe30 
x21: 00000055873e1720 x20: ffffff80011bc600 
x19: 0000000000000012 x18: 0000000000000000 
x17: 0000000000000000 x16: 0000000000000000 
x15: 0000000000000000 x14: 0000000000000000 
x13: 0000000000000000 x12: 0000000000000000 
x11: 0000000000000000 x10: 0000000000000000 
x9 : 0000000000000000 x8 : 0000000000000000 
x7 : 0000000000000000 x6 : 0000000000000000 
x5 : ffffff80020bfd98 x4 : ffffffc0086c7000 
x3 : ffffffc010ccbe30 x2 : 0000000000000012 
x1 : 0000000000000000 x0 : 0000000000000000 
Call trace:
 faulty_write+0x14/0x20 [faulty]
 ksys_write+0x68/0x100
 __arm64_sys_write+0x20/0x30
 el0_svc_common.constprop.0+0x94/0x1c0
 do_el0_svc+0x40/0xb0
 el0_svc+0x14/0x20
 el0_sync_handler+0x1a4/0x1b0
 el0_sync+0x174/0x180
Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
---[ end trace f15292901caaa1ca ]---
```

# Oops Message Analysis
The cause of the kernel error is shown by the line `Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000`. This indicates that a null pointer dereference of an incorrect pointer value was performed, also known as a page fault.

The exact line of code where execution stopped is shown by the line `faulty_write+0x14/0x20 [faulty]`. This indicates that the program counter was halted at the assembly instruction at address 14 in the faulty_write function. Objdump can now be used to further diagnose the problem.

# Objdump Output
```
faulty.ko:     file format elf64-littleaarch64

Disassembly of section .text:

0000000000000000 <faulty_write>:
   0:	d503245f 	bti	c
   4:	d2800001 	mov	x1, #0x0                   	// #0
   8:	d2800000 	mov	x0, #0x0                   	// #0
   c:	d503233f 	paciasp
  10:	d50323bf 	autiasp
  14:	b900003f 	str	wzr, [x1]
  18:	d65f03c0 	ret
  1c:	d503201f 	nop
```

# Objdump Analysis
The disassembly shows that the assembly instruction at address 14 is `str	wzr, [x1]`. This attempts to store a value at location 0 (the value of x1 is 0). This virtual address location of 0x00 is not accessible, which is why the faulty kernel driver results in an oops message. In the C code within faulty_write, this corresponds to the line dereferencing a null pointer: `*(int *)0 = 0`.
