#!/bin/bash

if [[ $# -ne 3 ]]; then
    echo "Usage: $0 <image_name> <container src_file> <host dst_file>"
    exit
fi

IMAGE_NAME=$1
SRC_FILE=$2
DST_FILE=$3

id=$(docker create $IMAGE_NAME)
docker cp $id:$SRC_FILE $DST_FILE
docker rm -v $id
