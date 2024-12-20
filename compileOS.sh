#/bin/bash
gcc loadFile.c -o loadFile

nasm bootload.asm
dd if=/dev/zero of=diskc.img bs=512 count=1000
dd if=bootload of=diskc.img bs=512 count=1 conv=notrunc

bcc -ansi -c -o kernel_c.o kernel.c
as86 kernel.asm -o kernel_asm.o
ld86 -o kernel -d kernel_c.o kernel_asm.o

./loadFile kernel
./loadFile message.txt

bcc -ansi -c -o tstpr1.o tstpr1.c
as86 -o userlib.o userlib.asm
ld86 -d -o tstpr1 tstpr1.o userlib.o
./loadFile tstpr1

bcc -ansi -c -o tstpr2.o tstpr2.c
as86 -o userlib.o userlib.asm
ld86 -o tstpr2 -d tstpr2.o userlib.o
./loadFile tstpr2

bcc -ansi -c -o shell_c.o shell.c
as86 userlib.asm -o userlib_asm.o
ld86 -o shell -d shell_c.o userlib_asm.o
./loadFile shell

bcc -ansi -c -o number_c.o number.c
ld86 -d -o number number_c.o userlib.o
./loadFile number

bcc -ansi -c -o letter_c.o letter.c
ld86 -d -o letter letter_c.o userlib.o
./loadFile letter