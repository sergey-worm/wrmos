#!/bin/bash
####################################################################################################
#
#  Sanity check for wrmos.
#
####################################################################################################

blddir=/tmp/wrm-test/wrmos

# CONFIG
hello_sparc_build=1
hello_sparc_exec=2
hello_arm_veca9_build=3
hello_arm_veca9_exec=4
hello_arm_zynqa9_build=5
hello_arm_zynqa9_exec=6
hello_x86_build=7
hello_x86_exec=8
hello_x86_64_build=9
hello_x86_64_exec=10
console_sparc_build=11
console_sparc_exec=12
console_arm_veca9_build=13
console_arm_veca9_exec=14
console_arm_zynqa9_build=15
console_arm_zynqa9_exec=16
console_x86_build=17
console_x86_exec=18
console_x86_64_build=19
console_x86_64_exec=20
result[$hello_sparc_build]=-
result[$hello_sparc_exec]=-
result[$hello_arm_veca9_build]=-
result[$hello_arm_veca9_exec]=-
result[$hello_arm_zynqa9_build]=-
result[$hello_arm_zynqa9_exec]=-
result[$hello_x86_build]=-
result[$hello_x86_exec]=-
result[$hello_x86_64_build]=-
result[$hello_x86_64_exec]=-
result[$console_sparc_build]=-
result[$console_sparc_exec]=-
result[$console_arm_veca9_build]=-
result[$console_arm_veca9_exec]=-
result[$console_arm_zynqa9_build]=-
result[$console_arm_zynqa9_exec]=-
result[$console_x86_build]=-
result[$console_x86_exec]=-
result[$console_x86_64_build]=-
result[$console_x86_64_exec]=-

res_ok='\e[1;32m+\e[0m'
res_bad='\e[1;31m-\e[0m'

errors=0

function get_result
{
	if [ $rc == 0 ]; then echo $res_ok; else echo $res_bad; fi
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
	if [ ${result[$id]} != $res_ok ]; then ((errors++)); fi
}

function do_exec
{
	id=$1
	prj=$2
	arch=$3
	brd=$4
	machine=$5

	qemu_args="-display none -serial stdio"
	qemu_args_x86="-serial stdio"
	run_qemu="qemu-system-$arch -M $machine $qemu_args -kernel $blddir/$prj-qemu-$brd/ldr/bootloader.elf"
	file=$blddir/$prj-qemu-$brd/ldr/bootloader.elf
	if [ $arch == x86 ]; then
		run_qemu="qemu-system-i386 $qemu_args_x86 -drive format=raw,file=$(realpath $blddir/$prj-qemu-$arch/ldr/bootloader.img)"
		file=$blddir/$prj-qemu-$brd/ldr/bootloader.img
	fi
	if [ $arch == x86_64 ]; then
		run_qemu="qemu-system-$arch $qemu_args_x86 -drive format=raw,file=$(realpath $blddir/$prj-qemu-$arch/ldr/bootloader.img)"
		file=$blddir/$prj-qemu-$brd/ldr/bootloader.img
	fi

	if [ -f $file ]; then
		if [ $prj == hello ]; then
			expect -c "\
				set timeout 10; \
				if { [catch {spawn $run_qemu} reason] } { \
					puts \"failed to spawn qemu: $reason\r\"; exit 1 }; \
				expect \"hello:  bye-bye.\r\" {}  timeout { exit 1 };
				expect \"terminated.\r\"      {}  timeout { exit 2 };
				exit 0"
			rc=$?
		else
		if [ $prj == console ]; then
			expect -c "\
				set timeout 10; \
				if { [catch {spawn $run_qemu} reason] } { \
					puts \"failed to spawn qemu: $reason\r\"; exit 1 }; \
				expect \"Input a chars or Enter to exit:\" {}  timeout { exit 1 }; \
				expect \"Input a chars or Enter to exit:\" {}  timeout { exit 2 }; \
				send bye-1\r;
				expect \"hello-1:  bye-bye.\r\"  {} timeout { exit 3 }; \
				send \x05; \
				expect \"new input app is *.\r\" {} timeout { exit 4 }; \
				send bye-2\r; \
				expect \"hello-2:  bye-bye.\r\"  {} timeout { exit 5 }; \
				expect \"terminated.\r\"         {} timeout { exit 2 };
				exit 0"
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
	if [ ${result[$id]} != $res_ok ]; then ((errors++)); fi
}

function do_all
{
	rm -fr $blddir

	# cmd     id                         prj      arch    brd     machine

	do_build  $hello_sparc_build         hello    sparc   leon3   leon3_generic
	do_exec   $hello_sparc_exec          hello    sparc   leon3   leon3_generic
	do_build  $hello_arm_veca9_build     hello    arm     veca9   vexpress-a9
	do_exec   $hello_arm_veca9_exec      hello    arm     veca9   vexpress-a9
	do_build  $hello_arm_zynqa9_build    hello    arm     zynqa9  xilinx-zynq-a9
	do_exec   $hello_arm_zynqa9_exec     hello    arm     zynqa9  xilinx-zynq-a9
	do_build  $hello_x86_build           hello    x86     x86     ""
	do_exec   $hello_x86_exec            hello    x86     x86     ""
	do_build  $hello_x86_64_build        hello    x86_64  x86_64  ""
	do_exec   $hello_x86_64_exec         hello    x86_64  x86_64  ""

	do_build  $console_sparc_build       console  sparc   leon3   leon3_generic
	do_exec   $console_sparc_exec        console  sparc   leon3   leon3_generic
	do_build  $console_arm_veca9_build   console  arm     veca9   vexpress-a9
	do_exec   $console_arm_veca9_exec    console  arm     veca9   vexpress-a9
	do_build  $console_arm_zynqa9_build  console  arm     zynqa9  xilinx-zynq-a9
	do_exec   $console_arm_zynqa9_exec   console  arm     zynqa9  xilinx-zynq-a9
	do_build  $console_x86_build         console  x86     x86     ""
	do_exec   $console_x86_exec          console  x86     x86     ""
	do_build  $console_x86_64_build      console  x86_64  x86_64  ""
	do_exec   $console_x86_64_exec       console  x86_64  x86_64  ""
}

do_all

echo -e "-------------------------------------------------"
echo -e "  REPORT:"
echo -e "-------------------------------------------------"
echo -e "  project  arch    machine         build  execute"
echo -e "- - - - - - - - - - - - - - - - - - - - - - - - -"
echo -e "  hello    sparc   leon3_generic       ${result[$hello_sparc_build]}        ${result[$hello_sparc_exec]}"
echo -e "  hello    arm     vexpress-a9         ${result[$hello_arm_veca9_build]}        ${result[$hello_arm_veca9_exec]}"
echo -e "  hello    arm     xilinx-zynq-a9      ${result[$hello_arm_zynqa9_build]}        ${result[$hello_arm_zynqa9_exec]}"
echo -e "  hello    x86                         ${result[$hello_x86_build]}        ${result[$hello_x86_exec]}"
echo -e "  hello    x86_64                      ${result[$hello_x86_64_build]}        ${result[$hello_x86_64_exec]}"
echo -e "  console  sparc   leon3_generic       ${result[$console_sparc_build]}        ${result[$console_sparc_exec]}"
echo -e "  console  arm     vexpress-a9         ${result[$console_arm_veca9_build]}        ${result[$console_arm_veca9_exec]}"
echo -e "  console  arm     xilinx-zynq-a9      ${result[$console_arm_zynqa9_build]}        ${result[$console_arm_zynqa9_exec]}"
echo -e "  console  x86                         ${result[$console_x86_build]}        ${result[$console_x86_exec]}"
echo -e "  console  x86_64                      ${result[$console_x86_64_build]}        ${result[$console_x86_64_exec]}"

echo -e "errors:  $errors"
exit $errors
