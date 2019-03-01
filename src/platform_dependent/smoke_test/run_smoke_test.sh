#!/bin/bash

OUTDIR=/pl_mount
SMOKETEST_PB=$OUTDIR/smoke_test.out
if apt-get install -y linux-headers-`uname -r` ; then
    # Run smoke_test with bcc functionality
    mount -t debugfs none /sys/kernel/debug/
    BCC_OUTPUT_FILE=$OUTDIR/smoke_test.bcc
    ./CPUDistribution_bcc 1 > $BCC_OUTPUT_FILE
    ./smoke_test -g -c -o $SMOKETEST_PB -b -f $BCC_OUTPUT_FILE
else
    # Run smoke test without bcc
    ./smoke_test -g -c -o  $SMOKETEST_PB
fi

# package everything into a tar
tar -cvf $OUTDIR/smoke_test.tar $OUTDIR/smoke_test.*
