#!/bin/bash

timeout 30 $SRC_DIR/test/test.py $SRC_DIR $BUILD_DIR
ret=$?

exit $ret

