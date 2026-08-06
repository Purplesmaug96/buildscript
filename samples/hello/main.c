// hello: print to the console
//
// A minimal OpenXeChain program. printf() is provided by Newlib and routes
// output through the kernel debug channel (the same trap DbgPrint uses), so
// the text shows up on debug consoles / kernel debug output rather than on
// the TV. Returning from main() exits back to the dashboard.

#include <stdio.h>

#include <xecore/xboxkrnl.h>

void main(void)
{
    printf("Hello world! (printf)\n");
	DbgPrint("Hello world! (DbgPrint)\n");
}
