## JVM stats

JVM stats are derived from JVM perfdata file. JVM perfdata is a feature to export a set of
predefined JVM stats to a memory mapped file at `/tmp/hsperfdata_<user>/<pid>`. We call this file
`hsperfdata file`.

JVM perfdata is enabled by default in all mainstream JVMs, including the `Azul Zing JVM` used
by Hulu. Azul Zing JVM exports a few more stats, and uses different names for some stats
(usually with `sun` replaced with `azul`). `-XX:+PerfDisableSharedMem` or `-XX:-UsePerfData` can be
used to disable this feature.

The format of the hsperfdata file is not publicly documented. It was discovered by reading the
source code of `jstat` (the official JDK utility to read the file) and `hsstat` (a rewrite in
Golang). Our C++ APIs works same as `hsstat`. The major difference compared to jstat is
that our APIs only support 2 data types: `long` and `string`. This works because there is no known
mainstream JVM that produces other types of data.

After reading the raw data from the hsperfdata file, additional conversions are applied to derive
more meaningful stats. We follow `jstat`'s spec at [1]. Stat names are the same as `jstat`.
See [man page](https://docs.oracle.com/javase/7/docs/technotes/tools/share/jstat.html) for
explanation of each stat.

### Specific column

used_heap_size: Sum of all the fields that end with "U", see https://stackoverflow.com/a/44095604.
total_heap_size: Sum of all the fields that end with "C".
max_heap_size: Sum of all the fields that end with "MX".

[1] http://hg.openjdk.java.net/jdk8u/jdk8u/jdk/file/8c93eb3fa1c0/src/share/classes/sun/tools/jstat/resources/jstat_options
