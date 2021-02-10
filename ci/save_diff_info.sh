#!/usr/bin/env bash

set -e

git diff -U0 origin/main > diff_origin_main
git diff -U0 origin/main -- '***.cc' '***.h' '***.c' > diff_origin_main_cc

git diff -U0 HEAD~10 > diff_head
git diff -U0 HEAD~10 -- '***.cc' '***.h' '***.c' > diff_head_cc
