#!/bin/bash 

cat test.csv | tr -d " \t\n\r" | tr , '\n' > test2.csv
cswave test2.csv test.wav 0 9600 i8
