#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/StealthCoin.ico

convert ../../src/qt/res/icons/StealthCoin-16.png ../../src/qt/res/icons/StealthCoin-32.png ../../src/qt/res/icons/StealthCoin-48.png ${ICON_DST}
