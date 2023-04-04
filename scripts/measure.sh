#!/bin/bash
CPUID=5
ORIG_ASLR=`cat /proc/sys/kernel/randomize_va_space`
ORIG_GOV=`cat /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor`
ORIG_TURBO=`cat /sys/devices/system/cpu/intel_pstate/no_turbo`
ORIG_SMT=`cat /sys/devices/system/cpu/smt/control`

sudo sh -c "echo 0 > /proc/sys/kernel/randomize_va_space"
sudo sh -c "echo performance > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"
sudo sh -c "echo 1 > /sys/devices/system/cpu/intel_pstate/no_turbo"
sudo sh -c "echo off > /sys/devices/system/cpu/smt/control"

python3 ./scripts/driver.py

sudo sh -c "echo $ORIG_ASLR >  /proc/sys/kernel/randomize_va_space"
sudo sh -c "echo $ORIG_GOV > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"
sudo sh -c "echo $ORIG_TURBO > /sys/devices/system/cpu/intel_pstate/no_turbo"
sudo sh -c "echo $ORIG_SMT > /sys/devices/system/cpu/smt/control"