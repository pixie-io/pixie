#!/bin/bash

wget https://raw.githubusercontent.com/torvalds/linux/v4.20/kernel/time/timeconst.bc

for hz in 100 250 300 1000; do
  echo "---------------------------"
  echo "Generating timeconst for HZ=$hz"
  timeconst_filename=timeconst_$hz.h
  gs_path="gs://pl-infra-dev-artifacts/$timeconst_filename"
  echo $hz | bc timeconst.bc > "$timeconst_filename"
  gsutil cp $timeconst_filename "$gs_path"
  gsutil acl ch -u allUsers:READER "$gs_path"
  sha256sum $timeconst_filename
  rm $timeconst_filename
done

rm timeconst.bc
