# malloc_overloader
Overload malloc() family using LD_PRELOAD technique

This is a shared library to LD_PRELOAD when testing a program. I created this based on some research on several sources.

Should be compiled as:  
`gcc -ggdb3 -Wall -Wextra -shared -fPIC -ldl -o liballoc.so liballoc.c`

Then you can test in you program, for example:  
`LD_PRELOAD=./liballoc.so ls -lart`

Example running gdb within malloc_overloader:  
`PATH=/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin LD_PRELOAD=./liballoc.so gdb /bin/ls`  
`gef➤  tbreak malloc`  
`gef➤  r`


![Screenshot from 2023-03-30 19-09-13](https://user-images.githubusercontent.com/8737680/228912701-c843993f-167c-4165-ab8b-398f8f0e769c.png)
