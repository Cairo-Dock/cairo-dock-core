#!/bin/bash
# Extract the description from readme files
# This script has to be be launched by bash and not sh...

#f=`cat "$1" | sed ':z;N;s/\n/\\n/;bz'`
cat "$1" | sed -e ':z;N;s/\n/\\n/;bz' > _tmp_
f=`cat _tmp_`
echo -E "_(\"$f\")" >> data/messages
rm -f _tmp_
