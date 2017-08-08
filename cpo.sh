#!/bin/sh

# This script is a hack that lets us copy individual .o files to the cmake
# output. This is so we can use crt0.o. As far as I can tell, there is no
# way to do this in cmake directly.
d=$1; shift
while [ "$1" != "--" ]; do
	cp $1 $d/$(echo $(basename $1) | sed "s/\.S\.o$/.o/"); shift
done
