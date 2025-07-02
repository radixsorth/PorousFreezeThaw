#!/bin/bash

TMPFILE1=$(mktemp -u /dev/shm/FREEZ1XXXXXXXXXXX)
TMPFILE2=$(mktemp -u /dev/shm/FREEZ2XXXXXXXXXXX)

# will save to the filename with the same name as the selected case
DEST=$(readlink test).txt

FORMULA="(p>0.5)*u"
#FORMULA="gl<0.5"

echo -n > "$DEST"

for FILE in test/OUTPUT/*.ncd
do
#    ncwa -v $VAR $FILE $TMPFILE
    echo Processing $FILE
    ncap2 -s "result=($FORMULA)" $FILE $TMPFILE1
    ncwa -y mabs -v result $TMPFILE1 $TMPFILE2
    ncdump -v result $TMPFILE2 | sed -n -e 's/^ 'result' = \(.\+\);/\1/p' >> "$DEST"
    rm $TMPFILE1
    rm $TMPFILE2
done
