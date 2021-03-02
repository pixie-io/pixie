# Socket tracer

Socket tracker deploys eBPF probes onto network IO syscalls (read/write, send/recv etc.),
captures data, and reassemble & parse them back into application-level protocol messages.

## Debugging

We often needs to debug:
* Protocol messages should have been traced, but didn't.
* Unexpected protocol messages.

The first step of debugging these issues is to verify the network IO behavior with
`tshark`/`wireshark` & `strace`.

* `tshark`: Use tshark to verify network traffic. `wireshark` is equivalent to tshark, but requires
   a windowing system like `X`. You can install tshark with:
  `sudo apt-get install tshark`. Or you could run it with a docker image.

  ```shell
  sudo docker run -it --rm --net container:CONTAINER_ID --privileged nicolaka/netshoot \
    tshark -f "src port 6379" -f "net IP" -Tjson -e ip -e tcp -e data
  ```

* `strace`: Use strace to verify syscalls are made, and examine their arguments. You can install
  strace with `sudo apt-get install strace`

  ```shell
  # -f is critical as it allows tracing all threads of a process.
  sudo strace -f --no-abbrev --attach=PID --string-limit=STRSIZE --trace=SYSCALL 2>&1 | grep PATTERN
  ```
