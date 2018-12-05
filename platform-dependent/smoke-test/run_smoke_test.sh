#!/bin/bash

if apt-get install -y linux-headers-`uname -r` ; then
    # Run smoke-test with bcc functionality
    mount -t debugfs none /sys/kernel/debug/
    # TODO(philkuz): Add bcc run option to smoke-test
    ./smoke-test -g -c -o /opt/usr/local/pl/smoke-test.out
else
    # Run smoke test without bcc
    ./smoke-test -g -c -o /opt/usr/local/pl/smoke-test.out
fi
