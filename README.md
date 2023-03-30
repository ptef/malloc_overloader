# malloc_overloader
Overload malloc() family using LD_PRELOAD technique

This is a shared library to LD_PRELOAD when testing a program. I created this based on some research on several sources.

Should be compiled as:  
`gcc -ggdb3 -Wall -Wextra -shared -fPIC -ldl -o liballoc.so liballoc.c`

Then you can test in you program, for example:  
`LD_PRELOAD=./liballoc.so ls -lart`
