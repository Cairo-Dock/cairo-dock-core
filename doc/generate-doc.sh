#!/bin/sh
NUMBER_RELEASE=`head -n 15 ../CMakeLists.txt | grep "set (VERSION" | cut -d\" -f2`
sed -i "s/PROJECT_NUMBER         = 0\.0\.0/PROJECT_NUMBER         = $NUMBER_RELEASE/g" dox.config
doxygen dox.config
sed -i "s/PROJECT_NUMBER         = $NUMBER_RELEASE/PROJECT_NUMBER         = 0.0.0/g" dox.config

echo ""
read -p "Do you want to produce a pdf by using LaTeX? [y/N] " answer
if test "$answer" = "y" -o "$answer" = "Y"; then
	cd latex
	make
	echo "A PDF should have been created: 'refman.pdf'"
	(xdg-open refman.pdf &) > /dev/null 2> /dev/null
	cd ..
fi
