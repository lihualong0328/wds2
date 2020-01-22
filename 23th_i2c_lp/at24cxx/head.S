@******************************************************************************
@ File: head.S
@ 功能: 设置SDRAM，将程序复制到SDRAM，然后跳到SDRAM继续执行
@******************************************************************************       
   
.extern     main
.text 
.global _start 
_start:
@******************************************************************************       
@ 中断向量，本程序中，除Reset和HandleIRQ外，其它异常都没有使用
@******************************************************************************       
    b   Reset   @ nand启动上电，nand的前4k被cp到片内内存 => 跳到Reset处执行

@ 0x04: 未定义指令中止模式的向量地址
HandleUndef:
    b   HandleUndef 
 
@ 0x08: 管理模式的向量地址，通过SWI指令进入此模式
HandleSWI:
    b   HandleSWI

@ 0x0c: 指令预取终止导致的异常的向量地址
HandlePrefetchAbort:
    b   HandlePrefetchAbort

@ 0x10: 数据访问终止导致的异常的向量地址
HandleDataAbort:
    b   HandleDataAbort

@ 0x14: 保留
HandleNotUsed:
    b   HandleNotUsed

@ 0x18: 中断模式的向量地址
    b   HandleIRQ

@ 0x1c: 快中断模式的向量地址
HandleFIQ:
    b   HandleFIQ

Reset:                  
    ldr sp, =4096           @ 设置栈指针，以下都是C函数，调用前需要设好栈
    
    @ 链接地址(已改为0x30000000) != 当前地址(片内4k)；所以4个函数必须用位置无关码来写(函数里不能用全局变量等)
    @ 因为全局变量是用链接地址去访问，但此时全局变量不在内存中(它的链接地址上)，nand_init()需要修改
    bl  disable_watch_dog   @ 关闭WATCHDOG，否则CPU会不断重启
    bl  clock_init          @ 设置MPLL，改变FCLK、HCLK、PCLK
    bl  memsetup            @ 设置存储控制器以使用SDRAM
    bl  nand_init           @ 初始化NAND Flash  @ 用位置无关码写
    
                            @ 重定位：复制代码到SDRAM中
    ldr r0, =0x30000000     @ 1. 目标地址 = 0x30000000，这是SDRAM的起始地址		@ 因为i2c.lds中链接地址，第二部分放到0x30000000
    mov r1, #0              @ 2. 源地址   = 0							//从nand的0(以前代码分成两段时是4096)地址开始copy
    ldr r2, =__bss_start    @ i2c.dis => 3000003c:	e59f2044 	ldr	r2, [pc, #68]	; 30000088 <.text+0x88>
                                        @ r2不是从30000088取值,而是相对位移,即r2=cur_pc+68=3c+8+68=88
    									@ 30000088:	30002bc0 	andcc	r2, r0, r0, asr #23
                                                    @ __bss_start地址30002bc0, 30002bc0-链接地址=2bc0, 即i2c.bin的大小
    												
    sub r2, r2, r0          @ 3. 复制长度，没必要把bss放到bin(二进制文件)文件里
    bl  CopyCode2SDRAM      @ 调用C函数CopyCode2SDRAM 	bl是相对跳转						// 重定位
    
    bl  clean_bss           @ 清除bss段，未初始化或初值为0的全局/静态变量保存在bss段

    msr cpsr_c, #0xd2       @ 进入中断模式
    ldr sp, =0x31000000     @ 设置中断模式栈指针

    msr cpsr_c, #0xdf       @ 进入系统模式
    ldr sp, =0x34000000     @ 设置系统模式栈指针，

    ldr lr, =ret_initirq    @ 设置返回地址    
    ldr pc, =init_irq       @ 调用中断初始化函数
ret_initirq:
    msr cpsr_c, #0x5f       @ 设置I-bit=0，开IRQ中断

    ldr lr, =halt_loop      @ 设置返回地址
    ldr pc, =main           @ 调用main函数
halt_loop:
    b   halt_loop

HandleIRQ:
    sub lr, lr, #4                  @ 计算返回地址
    stmdb   sp!,    { r0-r12,lr }   @ 保存使用到的寄存器
                                    @ 注意，此时的sp是中断模式的sp
                                    @ 初始值是上面设置的4096
    
    ldr lr, =int_return             @ 设置调用IRQ_Handle函数后的返回地址  
    ldr pc, =IRQ_Handle             @ 调用中断分发函数，在interrupt.c中
int_return:
    ldmia   sp!,    { r0-r12,pc }^  @ 中断返回, ^表示将spsr的值复制到cpsr
    
