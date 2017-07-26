static void __attribute__ ((noinline)) readTimeBaseReg(unsigned long *tbu, unsigned long *tbl)
{
    register unsigned long dummy __asm__ ("r0") = 0;
    __asm__ __volatile__(
                         "loop:	mftbu %2\n"
			 "	mftb  %0\n"
			 "	mftbu %1\n" 
			 "	cmpw  %1, %2\n"
			 "	bne   loop\n"
			 : "=r"(*tbl), "=r"(*tbu), "=r"(dummy)
			 : "1"(*tbu), "2"(dummy)
    );
}
