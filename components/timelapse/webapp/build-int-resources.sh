#!/bin/sh

#
# Move all required files to "build" folder, compressing where possible
#

rm -rf build/*

#purifycss  *.css index.htm --min > build/combined.min.css # drops too many clases

# CSS
cat  custom.css wireframe.css > build/combined.css
yui-compressor -o build/combined.min.css build/combined.css
rm build/combined.css

# JS excl. ACE
touch build/combined.min.js
cat js/jquery.slim.min.js >> build/combined.min.js
cat js/bootstrap.bundle.min.js >> build/combined.min.js

yui-compressor -o build/populate.min.js js/populate.js
cat build/populate.min.js >> build/combined.min.js
rm build/populate.min.js

yui-compressor -o build/ws_events_dispatcher.min.js js/ws_events_dispatcher.js
cat build/ws_events_dispatcher.min.js >> build/combined.min.js
rm build/ws_events_dispatcher.min.js

yui-compressor -o build/formToObject.min.js js/formToObject.js
cat build/formToObject.min.js >> build/combined.min.js
rm build/formToObject.min.js

# This codebase - leave uncompressed for debugging
cp script.js *.htm* build/

# ACE is seperate
cp -r js/ace build/
