#!/bin/bash

OUTDIR=/pl_mount
SMOKETEST_PB=$OUTDIR/smoke-test.out
if apt-get install -y linux-headers-`uname -r` ; then
    # Run smoke-test with bcc functionality
    mount -t debugfs none /sys/kernel/debug/
    BCC_OUTPUT_FILE=$OUTDIR/smoke-test.bcc
    # TODO(philkuz): Add bcc run option to smoke-test
    ./CPUDistribution_bcc 1 > $BCC_OUTPUT_FILE
    ./smoke-test -g -c -o $SMOKETEST_PB -b -f $BCC_OUTPUT_FILE
else
    # Run smoke test without bcc
    ./smoke-test -g -c -o  $SMOKETEST_PB
fi

# package everything into a tar
tar -cvf $OUTDIR/smoke-test.tar $OUTDIR/smoke-test.*
