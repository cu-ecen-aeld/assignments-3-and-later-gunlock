#!/bin/bash

# 2 argments must provided
if [ $# -lt 2 ]; then
    echo "Error. Both the file path and string must be provided"
    exit 1
fi

# set vars to args
writefile=$1
writestr=$2

# Extract the directory name
dir=$(dirname "$writefile")

# create directory and any subdirectory with -p
mkdir -p "$dir"

# check last exit code from mkdir, $?, for success (0 == success)
if [ $? -ne 0 ]; then
    echo "Error. Failed to create directory"
    exit 1
fi

# write string to file. this will overwrite existing file
echo "$writestr" > "$writefile"

# check for success by checking the exit code ($?) from the write
if [ $? -ne 0 ]; then
    echo "Error. Failed to create file."
    exit 1
fi
