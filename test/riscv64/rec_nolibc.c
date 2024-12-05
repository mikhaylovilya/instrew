//#include <stdio.h>    
//#include <stdint.h>    
     
int rec(int n) {    
    if (n <= 0)    
        return 1;    
    return n * rec(n - 1);     
}    
     
__attribute__((force_align_arg_pointer))    
void _start() {    
    int n = 5;    
//  printf("rec(%d) = %d\n", n, rec(n));    
    int res = rec(n);    
    asm("lb a0, %0\n"    
        "li a7, 93\n"    
        "ecall\n" :: "m"(res)    
    );      
    __builtin_unreachable();    
}    

