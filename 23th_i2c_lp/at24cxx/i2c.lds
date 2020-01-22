/* 
	*
	*SECTIONS {
	*     . = 0x00000000;
	*     .init : AT(0){ head.o init.o nand.o}	@ .init必须存在的字段，是段名
	*     . = 0x30000000;	@ 段的重定位地址，即本段在内存中链接（运行）时的地址，一般也是需要指出的
	*     .text : AT(4096) { *(.text) }	@ AT是本段存储加载地址（在生成的文件中的地址）
	*     .rodata ALIGN(4) : AT((LOADADDR(.text)+SIZEOF(.text)+3)&~(0x03)) {*(.rodata*)}	@ 内存对其的方式			
	*     .data ALIGN(4)   : AT((LOADADDR(.rodata)+SIZEOF(.rodata)+3)&~(0x03)) { *(.data) }	@ 本段内容，可以是整个目标文件/目标文件中的某段（代码段，数据段等）
	*     __bss_start = .;																																						@ 链接时确定__bss_start地址
	*     .bss ALIGN(4)  : { *(.bss)  *(COMMON) }
	*      __bss_end = .;
	*}
	* 如果没有用AT指令给出该段存放的地址，那么该段就会放在和运行地址相同的地方，他们只是地址相同，一个在内存，一个在生成的可执行文件里面 => 程序运行时就需将加载地址的代码拷贝到链接地址(内存)去运行
	* AT((LOADADDR(.text)+SIZEOF(.text)+3)&~(0x03)) => (代码段的位置+代码段的长度+3)&~(0x03) => "+3)&~(0x03)" => 为了4字节对齐
	* 
	* 以前代码被分为两段，不用这么复杂，其实一段也行(链接地址都是0x30000000)
*/



SECTIONS {																				
    . = 0x30000000;
    .text : { head.o(.text)	/* i2c.bin > 10K, 在重定位前, head.o & init.o & nand.o 必须位于片内sram（前4k） */
              init.o(.text)	/* 4k=>4096=>1000 : vi i2c.dis => 30001000:	e3500000 	cmp	r0, #0	; 0x0 */
              nand.o(.text)	/* nand.c 最后函数 nand_read 的链接地址 3000038c => 3000038c <nand_read>: 3000038c < 4k */
              *(.text) 
            }
    .rodata ALIGN(4) : {*(.rodata*)} 
    .data   ALIGN(4) : { *(.data) }
    __bss_start = .;
    .bss ALIGN(4)  : { *(.bss)  *(COMMON) }
    __bss_end = .;
}





