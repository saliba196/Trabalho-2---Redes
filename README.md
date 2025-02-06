luiza_sudo@ubuntu:~/Downloads$ cd T2
luiza_sudo@ubuntu:~/Downloads/T2$ ls
cliente.c  makefile  rdt_2.2.c  rdt.h  servidor.c
luiza_sudo@ubuntu:~/Downloads/T2$ make
gcc -Wall -Wextra -std=c99 -O2 -o cliente cliente.c rdt_2.2.c
cliente.c: In function ‘main’:
cliente.c:62:13: warning: implicit declaration of function ‘rdt_recv_static’ [-Wimplicit-function-declaration]
   62 |         if (rdt_recv_static(s, &msg, sizeof(msg), &caddr) < 0)
      |             ^~~~~~~~~~~~~~~
cliente.c:73:1: error: stray ‘\302’ in program
   73 |     return 0;
      | ^
cliente.c:73:2: error: stray ‘\302’ in program
   73 |     return 0;
      |  ^
cliente.c:73:3: error: stray ‘\302’ in program
   73 |     return 0;
      |   ^
cliente.c:73:4: error: stray ‘\302’ in program
   73 |     return 0;
      |    ^
cliente.c:73:11: error: stray ‘\302’ in program
   73 |     return 0;
      |           ^
cliente.c:22:15: warning: unused variable ‘addrlen’ [-Wunused-variable]
   22 |     socklen_t addrlen = sizeof(caddr);
      |               ^~~~~~~
make: *** [makefile:20: cliente] Error 1
luiza_sudo@ubuntu:~/Downloads/T2$ make clean
rm -f cliente servidor
luiza_sudo@ubuntu:~/Downloads/T2$ make
gcc -Wall -Wextra -std=c99 -O2 -o cliente cliente.c rdt_2.2.c
cliente.c: In function ‘main’:
cliente.c:62:13: warning: implicit declaration of function ‘rdt_recv_static’ [-Wimplicit-function-declaration]
   62 |         if (rdt_recv_static(s, &msg, sizeof(msg), &caddr) < 0)
      |             ^~~~~~~~~~~~~~~
cliente.c:73:1: error: stray ‘\302’ in program
   73 |     return 0;
      | ^
cliente.c:73:2: error: stray ‘\302’ in program
   73 |     return 0;
      |  ^
cliente.c:73:3: error: stray ‘\302’ in program
   73 |     return 0;
      |   ^
cliente.c:73:4: error: stray ‘\302’ in program
   73 |     return 0;
      |    ^
cliente.c:73:11: error: stray ‘\302’ in program
   73 |     return 0;
      |           ^
cliente.c:22:15: warning: unused variable ‘addrlen’ [-Wunused-variable]
   22 |     socklen_t addrlen = sizeof(caddr);
      |               ^~~~~~~
make: *** [makefile:20: cliente] Error 1
