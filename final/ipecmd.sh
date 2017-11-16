#!/bin/bash

args=""

for arg in "$@"; do
	if [[ "${arg:0:2}" = "-F" ]]
	then
		file=${arg:2}
 
		if [[ "${file:0:1}" != "/" ]]
		then
			file="$(pwd)/$file"
		fi
		
		args+="-F$file "
	else
		args+="$arg "
	fi
done

if [ ! -f ~/ipec-deps/ipecmd.jar ]; then
    echo "Could not find ~/ipec-deps/ipecmd.jar: please download https://cornell.box.com/s/8aqitoj0c5vtj689i8u1i5aqp8c3wa6m and unzip in the home directory"
    exit 1
fi

java -jar ~/ipec-deps/ipecmd.jar $args

