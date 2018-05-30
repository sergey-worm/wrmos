# wrmos

RTOS based on L4 microkernel.

## Description

WrmOS - RTOS based on L4 microkernel.

WrmOS includes:

* kernel       - mikrokernel based on "L4 kernel reference manual";
* applications - sigma0 (rootpager), alpha (roottask), tcpip, etc;
* libraries    - wlibc, wstdc++, psockets, elfloader, etc.
* build system - based on config files and makefiles (dirs cfg/ and mk/);
* bootloader   - bootstrap the system, parse ramfs and load the kernel.

## How to

Build and run:

	qemu-sparc-leon3:
		make build P=cfg/prj/hello-qemu-leon3.prj B=../build -j
		qemu-system-sparc -M leon3_generic -display none -serial stdio \
			-kernel ../build/ldr/bootloader.elf

	tsim-leon3:
		make build P=cfg/prj/hello-tsim-leon3.prj B=../build -j
		tsim-leon3 -mmu -fast_uart ../build/ldr/bootloader.elf

	qemu-arm-vexpress-a9:
		make build P=cfg/prj/hello-qemu-veca9.prj B=../build -j
		qemu-system-arm -M vexpress-a9 -display none -serial stdio \
			-kernel ../build/ldr/bootloader.elf

	qemu-x86:
		make build P=cfg/prj/hello-qemu-x86.prj B=../build -j
		qemu-system-i386 -display none -serial stdio \
			-drive format=raw,file=$(realpath ../build/ldr/bootloader.img)

	qemu-x86_64:
		make build P=cfg/prj/hello-qemu-x86_64.prj B=../build -j
		qemu-system-x86_64 -display none -serial stdio \
			-drive format=raw,file=$(realpath ../build/ldr/bootloader.img)

	Raspberry PI:
		make build P=cfg/prj/hello-rpi.prj B=../build -j
		TODO:  how to run ?

Autotest (sanity check):

	mk/test.sh

## Project state

Now supportes 4 archs:  SPARC, ARM, x86, x86_64.
On SPARC successfull works complex real time projects.
On other archs works just simple examples like HelloWorld.

WrmOS have own implementation of next components:
- libc   - lib/wlibc (some basic functions);
- stdc++ - lib/stdc++ (few basic functions);
- tcp/ip - tcpip server app/tcpip (arp, udp, tcp, ip, ...);

## Plans

1. Run WrmOS on real hardware for x86 and x86_64;
1. Support SMP;
1. Support ROS API (ros.org);
1. Support OROCOS API (orocos.org).

## Contacts

Sergey Worm <sergey.worm@gmail.com>

