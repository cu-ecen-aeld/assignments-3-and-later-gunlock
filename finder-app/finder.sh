#!/bin/bash

# exit if both arguments are not provided
if [ $# -lt 2 ]; then
    echo "Error. Both the directory and search string parameters must be provided"
    exit 1
fi

# Set variables to args
path=$1
pattern=$2

# Check if directory exists
if [ ! -d "$path" ]; then
    echo "Error. Directory does not exist"
    exit 1
fi

# Get file count by piping output of grep to wc
# grep
#  - r recursive
#  - l list file names only
# wc - print newline, word, byte count
#  -l lines
filecount=$(grep -rl "$pattern" "$path" | wc -l)

# Get line count
linecount=$(grep -r "$pattern" "$path" | wc -l)

echo "The number of files are $filecount and the number of matching lines are $linecount"
