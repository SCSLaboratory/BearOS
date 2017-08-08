#!/bin/sh
# Script for running fdisk on the bear hard disk image, which should
# be in /dev/loop0. Eventually we'll change that to be more flexible.
#
# The hard drive should have already had the MBR and boot1 code put
# into it via dd. Then this runs, which initializes the partition
# table (one slice, stretching from sector 2048 to the end of the
# disk), and the disklabel (one partition, covering the entire
# slice).
#
# This doesn't initialize the length of the boot code in the
# disklabel, nor does it put the start of the partition after the 
# boot code like it should be. We'll have to change that when we have
# boot code to put there, possibly with our own disklabel utility.
# I'm pretty sure that Linux fdisk doesn't recognize disklabels with
# variable number of partition entries either, so that's yet more
# reason to ditch it as soon as we can.
#
# Newer versions of fdisk require a slightly different sequence of
# commands.  As of April 2011, standard installations of Ubuntu still
# include an older version that requires the old command sequence.
# This is a SERIOUS HACK; if there's another update to fdisk this
# will likely break.
if fdisk -v | grep -q 2.17.2; then
fdisk $1 << EOF
u
c
n
p
1
${PART_OFFSET}

a
1
t
a5
w
EOF
else
fdisk /dev/loop0 << EOF
n
p
1
${PART_OFFSET}

a
1
t
a5
w
EOF
fi
