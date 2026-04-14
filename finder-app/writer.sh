#!/bin/sh


# Check if both arguments are provided
if [ $# -lt 2 ]; then
    echo "Error: Two arguments required."
    echo "Usage: ./writer.sh <writefile> <writestr>"
    exit 1
fi

writefile=$1
writestr=$2

# Extract the directory path from the full file path
dirpath=$(dirname "$writefile")

# Create the directory path if it doesn't exist
if ! mkdir -p "$dirpath"; then
    echo "Error: Could not create directory path $dirpath."
    exit 1
fi

# Attempt to write the string to the file

if ! echo "$writestr" > "$writefile"; then
    echo "Error: File could not be created or written to at $writefile."
    exit 1
fi

# Exit successfully
exit 0
