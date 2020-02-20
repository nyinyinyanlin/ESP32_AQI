#!/bin/bash

cleancss -o ../unzipped/bundle.css ../unbundled/*.css
cat \
../unbundled/jquery.js \
../unbundled/bootstrap.js \
../unbundled/apexcharts.js \
../unbundled/popper.min.js | terser -c -m -o ../unzipped/bundle.js
