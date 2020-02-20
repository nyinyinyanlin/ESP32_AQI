#!/bin/bash

for file in ../unzipped/*; do
	gzip -c $file > ../data/"$(basename "$file")".gz
done
