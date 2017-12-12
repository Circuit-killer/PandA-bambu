#!/bin/bash
abs_script=$(readlink -e $0)
dir_script=$(dirname $abs_script)
bambu $dir_script/mips.c --memory-ctrl-type=D21 --channels-type=MEM_ACC_NN --memory-allocation-policy=EXT_PIPELINED_BRAM --channels-number=4 --clock-period=15 --simulate -v3

