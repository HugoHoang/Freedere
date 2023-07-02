#!/bin/bash
echo -e "analyzing file $1"
pathToExec=${1%.*}
execName=${pathToExec#*/}
echo -e "executable $execName"
g++ -o $execName -Wall -ggdb3 $1
valgrind --leak-check=full  $pathToExec 2> vgout.txt
cat vgout.txt | cut -f2- -d' ' > vg_analysis.txt

