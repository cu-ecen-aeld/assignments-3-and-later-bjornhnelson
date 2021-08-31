#!/bin/bash
# Author: Bjorn Nelson

# save command line arguments
writefile=$1
writestr=$2

# verify that there are exactly 2 arguments
if [ $# -ne 2 ]
then
	echo "ERROR: invalid number of arguments"
	exit 1
fi

# get path without file name
directory_name=$(dirname $writefile)

# get file name without prepended path
file_name=$(basename $writefile)

# create directory, if it does not already exist
if [ ! -d $directory_name ]
then
	mkdir -p $directory_name

	if [ ! -d $directory_name ]
	then
		echo "ERROR: could not create directory"
		exit 1
	fi
fi

# write to file
cd $directory_name
echo $writestr > $file_name

# verify that file was written to
if [ $? -eq 0 ]
then
	echo "File written to successfully"
	exit 0
else
	echo "ERROR: file not written to"
	exit 1
fi

