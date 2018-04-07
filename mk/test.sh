#!/bin/bash
####################################################################################################
#
#  Sanity check wrmos.
#
####################################################################################################

blddir=/tmp/wrm-test/wrmos

# CONFIG
hello_sparc_build=1
hello_sparc_exec=2
hello_arm_build=3
hello_arm_exec=4
hello_x86_build=5
hello_x86_exec=6
hello_x86_64_build=7
hello_x86_64_exec=8
console_sparc_build=9
console_sparc_exec=10
console_arm_build=11
console_arm_exec=12
console_x86_build=13
console_x86_exec=14
console_x86_64_build=15
console_x86_64_exec=16
result[$hello_sparc_build]=-
result[$hello_sparc_exec]=-
result[$hello_arm_build]=-
result[$hello_arm_exec]=-
result[$hello_x86_build]=-
result[$hello_x86_exec]=-
result[$hello_x86_64_build]=-
result[$hello_x86_64_exec]=-
result[$console_sparc_build]=-
result[$console_sparc_exec]=-
result[$console_arm_build]=-
result[$console_arm_exec]=-
result[$console_x86_build]=-
result[$console_x86_exec]=-
result[$console_x86_64_build]=-
result[$console_x86_64_exec]=-

function get_result
{
	if [ $rc == 0 ]; then echo '\e[1;32m+\e[0m'; else echo '\e[1;31m-\e[0m'; fi
}

function do_build
{
	id=$1
	prj=$2
	arch=$3
	brd=$4
	time make build P=cfg/prj/${prj}-qemu-${brd}.prj B=$blddir/$prj-qemu-${brd} -j
	rc=$?
	echo "Build:  rc=$rc."
	result[$id]=$(get_result $rc)
}

function do_exec
{
	id=$1
	prj=$2
	arch=$3
	brd=$4
	machine=$5

	qemu_args="-display none -serial stdio"
	run_qemu="qemu-system-$arch -M $machine $qemu_args -kernel $blddir/$prj-qemu-$brd/ldr/bootloader.elf"
	file=$blddir/$prj-qemu-$brd/ldr/bootloader.elf
	if [ $arch == x86 ]; then
		run_qemu="qemu-system-i386 $qemu_args -drive format=raw,file=$(realpath $blddir/$prj-qemu-$arch/ldr/bootloader.img)"
		file=$blddir/$prj-qemu-$brd/ldr/bootloader.img
	fi
	if [ $arch == x86_64 ]; then
		run_qemu="qemu-system-$arch $qemu_args -drive format=raw,file=$(realpath $blddir/$prj-qemu-$arch/ldr/bootloader.img)"
		file=$blddir/$prj-qemu-$brd/ldr/bootloader.img
	fi

	if [ -f $file ]; then
		if [ $prj == hello ]; then
			expect -c "\
				set timeout 10; \
					if { [catch {spawn $run_qemu} reason] } { \
						puts \"failed to spawn qemu: $reason\r\"; exit 1 }; \
				expect \"hello:  bye-bye.\r\" { exit 0 }  timeout { exit 1 };  exit 2"
			rc=$?
		else
		if [ $prj == console ]; then
			expect -c "\
				set timeout 10; \
				if { [catch {spawn $run_qemu} reason] } { \
					puts \"failed to spawn qemu: $reason\r\"; exit 1 }; \
				expect \"Input a chars or Enter to exit:\" { }  timeout { exit 1 }; \
				expect \"Input a chars or Enter to exit:\" { }  timeout { exit 2 }; \
				send bye-1\r;
				expect \"hello-1:  bye-bye.\r\" { }  timeout { exit 3 }; \
				send \x05; \
				expect \"new input app is *.\r\" { }  timeout { exit 4 } }; \
				send bye-2\r; \
				expect \"hello-2:  bye-bye.\r\" { exit 0 }  timeout { exit 5 }; \
				exit 6"
			rc=$?
		else
			rc=100  # unknown project
		fi
		fi
	else
		rc=200  # no exec file
	fi

	echo -e "\nExecute:  rc=$rc."
	result[$id]=$(get_result $rc)
}

function do_all
{
	rm -fr $blddir

	# cmd     id                     prj      arch    brd     machine

	do_build  $hello_sparc_build     hello    sparc   leon3   leon3_generic
	do_exec   $hello_sparc_exec      hello    sparc   leon3   leon3_generic
	do_build  $hello_arm_build       hello    arm     veca9   vexpress-a9
	do_exec   $hello_arm_exec        hello    arm     veca9   vexpress-a9
	do_build  $hello_x86_build       hello    x86     x86     ""
	do_exec   $hello_x86_exec        hello    x86     x86     ""
	do_build  $hello_x86_64_build    hello    x86_64  x86_64  ""
	do_exec   $hello_x86_64_exec     hello    x86_64  x86_64  ""

	do_build  $console_sparc_build   console  sparc   leon3   leon3_generic
	do_exec   $console_sparc_exec    console  sparc   leon3   leon3_generic
	do_build  $console_arm_build     console  arm     veca9   vexpress-a9
	do_exec   $console_arm_exec      console  arm     veca9   vexpress-a9
	do_build  $console_x86_build     console  x86     x86     ""
	do_exec   $console_x86_exec      console  x86     x86     ""
	do_build  $console_x86_64_build  console  x86_64  x86_64  ""
	do_exec   $console_x86_64_exec   console  x86_64  x86_64  ""
}

do_all

echo -e "---------------------------------------------"
echo -e "  REPORT:"
echo -e "---------------------------------------------"
echo -e "  project  arch    build  execute"
echo -e "- - - - - - - - - - - - - - - - - - - - - - -"
echo -e "  hello    sparc       ${result[$hello_sparc_build]}        ${result[$hello_sparc_exec]}"
echo -e "  hello    arm         ${result[$hello_arm_build]}        ${result[$hello_arm_exec]}"
echo -e "  hello    x86         ${result[$hello_x86_build]}        ${result[$hello_x86_exec]}"
echo -e "  hello    x86_64      ${result[$hello_x86_64_build]}        ${result[$hello_x86_64_exec]}"
echo -e "  console  sparc       ${result[$console_sparc_build]}        ${result[$console_sparc_exec]}"
echo -e "  console  arm         ${result[$console_arm_build]}        ${result[$console_arm_exec]}"
echo -e "  console  x86         ${result[$console_x86_build]}        ${result[$console_x86_exec]}"
echo -e "  console  x86_64      ${result[$console_x86_64_build]}        ${result[$console_x86_64_exec]}"
