#!/bin/bash
# Author: Bjorn Nelson

# save command line arguments
filesdir=$1
searchstr=$2

# verify that there are exactly 2 arguments
if [ $# -ne 2 ]
then
	echo "ERROR: invalid number of arguments"
	exit 1
fi

# verify that the path is valid
if [ ! -d $filesdir ]
then
	echo "ERROR: invalid search path"
	exit 1
fi

# run the scripts
num_files=$(find $filesdir -type f | wc -l)
num_matches=$(grep -r "$searchstr" $filesdir | wc -l)

# print the results
echo "The number of files are ${num_files} and the number of matching lines are ${num_matches}"
exit 0

