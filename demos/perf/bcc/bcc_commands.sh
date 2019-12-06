#!/bin/bash

interval_secs=2
capture_secs=10
outdir="output"
usage()
{
  echo "Usage: $0 [-i interval in secs to store data] [-t time for capture in seconds] \
  [-o output directory under /usr/share/bcc/tools (will be extended with a timestamp)]"
  exit 2
}

# Input options
# interval_secs is the interval at which data should be printed to the output file
# capture_secs is the duration for which data should be collected
# outdir is the directory under /usr/share/tools/bcc where output data will be stored

while getopts 'hi:t:o:' c
do
    case $c in
        h) usage ;;
        i) interval_secs=$OPTARG
            echo "Script will collect data at intervals of $interval_secs secs."
           ;;
        t) capture_secs=$OPTARG
            echo "Script will collect data for $capture_secs secs. before exiting"
           ;;
        o) outdir=${OPTARG}_$(date +%Y_%m_%d_%H_%M_%S)
            echo "Results will be stored in /usr/share/bcc/tools/$outdir"
           ;;
        :) usage ;;
    esac
done

mount -t debugfs none /sys/kernel/debug
mkdir -p bcc_data
mkdir -p "$outdir"

env > ./"$outdir"/env.txt
timeout -s SIGINT "$capture_secs" ./cachestat -T "$interval_secs" > ./"$outdir"/cachestat.txt &
timeout -s SIGINT "$capture_secs" ./execsnoop > ./"$outdir"/execsnoop.txt &
./opensnoop -d "$capture_secs" > ./"$outdir"/opensnoop.txt &
timeout -s SIGINT "$capture_secs" ./ext4slower > ./"$outdir"/ext4slower.txt &
timeout -s SIGINT "$capture_secs" ./biolatency > ./"$outdir"/biolatency.txt &
timeout -s SIGINT "$capture_secs" ./biosnoop > ./"$outdir"/biosnoop.txt &
timeout -s SIGINT "$capture_secs" ./tcpconnect > ./"$outdir"/tcpconnect.txt &
timeout -s SIGINT "$capture_secs" ./tcpaccept > ./"$outdir"/tcpaccept.txt &
timeout -s SIGINT "$capture_secs" ./tcpretrans > ./"$outdir"/tcpretrans.txt &
timeout -s SIGINT "$capture_secs" ./runqlat > ./"$outdir"/runqlat.txt &
timeout -s SIGINT "$capture_secs" ./profile > ./"$outdir"/profile.txt &
timeout -s SIGINT "$capture_secs" ./funccount 'vfs_*' > ./"$outdir"/funccount_vfs.txt &
timeout -s SIGINT "$capture_secs" ./funccount 'c:malloc_*' > ./"$outdir"/funccount_malloc.txt &

sleep `expr $capture_secs + 30`
mkdir -p bcc_data/"$outdir"
# gcsfuse takes its sweet time to create a directory
sleep 10
cp -rf "$outdir"/*.txt bcc_data/"$outdir"/
sync -f bcc_data/"$outdir"/*.txt
