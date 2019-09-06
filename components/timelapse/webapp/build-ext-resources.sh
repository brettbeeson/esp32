#!/bin/sh

#
# Move all required files to "build" folder, compressing where possible
#

rm -rf build/*

# drops too many clases
# todo: improve and allow
#purifycss  *.css index.htm --min > build/combined.min.css

# CSS
cat  custom.css wireframe.css > build/combined.css
yui-compressor -o build/combined.min.css build/combined.css
rm build/combined.css

touch build/combined.min.js

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