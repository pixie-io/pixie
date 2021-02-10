# DNS Server image

This directory contains the build files for a standalone DNS server in a container.

The build of the DNS server is all managed by bazel. A description of the files is included below:

 - named.conf: A configuration file for the bind9 DNS server; gets added to the image by Bazel.
 - dnstest.com.zone: Name resolutions managed by this DNS server; gets added to the image by Bazel.
                     This file sets up two DNS entries:
                         server.dnstest.com ->  192.168.32.200
                         client.dnstest.com ->  192.168.32.201
