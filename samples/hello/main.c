// hello: print to the console
//
// A minimal OpenXeChain program. printf() is provided by Newlib and routes
// output through the kernel debug channel (the same trap DbgPrint uses), so
// the text shows up on debug consoles / kernel debug output rather than on
// the TV. Returning from main() exits back to the dashboard.

#include <stdio.h>

void main(void)
{
    printf("Hello, Xbox 360!\n");
    printf("This was built with OpenXeChain.\n");
    printf("int=%d hex=%x float=%.2f pointer=%p\n",
           42, 0xCAFE, 3.14159, (void *)main);

    unsigned long long big = 0x1122334455667788ULL;
    printf("unsigned long long=%llu\n", big);

    for (int i = 0; i < 5; i++)
        printf("tick %d\n", i);
}
