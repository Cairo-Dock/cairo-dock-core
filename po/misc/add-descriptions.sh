#!/bin/bash
# Extract the description from auto-load.conf file
# This script has to be be launched by bash and not sh...

description=`grep "^description =" auto-load.conf | cut -d= -f2`

# removed spaces at the beginning
while [ "${description:0:1}" = " " ]; do
	description="${description:1:${#description}}"
done

# replaced " by \"
description=`echo -E $description | sed -e 's/"/\\\"/g; s/\r//g'`

# add in the messages file
echo "" >> messages
echo -E "_(\"$description\")" >> messages
