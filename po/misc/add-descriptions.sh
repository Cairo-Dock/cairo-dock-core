#!/bin/bash
# Extract the a key ($1) from auto-load.conf file
# This script has to be be launched by bash and not sh...

description=`grep "^$1 =" auto-load.conf | cut -d= -f2`

if test -z "$description"; then
	# echo "This applet doesn't have any $1"
	exit 0
fi

# removed spaces at the beginning
while [ "${description:0:1}" = " " ]; do
	description="${description:1:${#description}}"
done

if test -z "$description"; then
	# echo "This applet doesn't have any $1"
	exit 0
fi

# replaced " by \"
description=`echo -E $description | sed -e 's/"/\\\"/g; s/\r//g'`

# add in the messages file
echo "" >> messages
echo -E "_(\"$description\")" >> messages
