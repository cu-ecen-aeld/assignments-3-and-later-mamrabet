#!/bin/sh

# Check if both arguments are provided
if [ $# -lt 2 ]; then
    echo "Error: Two arguments required."
    echo "Usage: ./finder.sh <filesdir> <searchstr>"
    exit 1
fi

filesdir=$1
searchstr=$2

# Check if filesdir is a valid directory
if [ ! -d "$filesdir" ]; then
    echo "Error: Directory '$filesdir' does not exist on the filesystem."
    exit 1
fi

# Calculate X: Number of files in the directory and subdirectories
X=$(find "$filesdir" -type f | wc -l)

# Calculate Y: Number of matching lines across all files
Y=$(grep -r "$searchstr" "$filesdir" | wc -l)

# Print the final message
echo "The number of files are $X and the number of matching lines are $Y"
