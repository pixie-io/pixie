# Common functions and imports used by the WORKSPACE file.

# From tensorflow/workspace.bzl.
def _parse_bazel_version(bazel_version):
  # Remove commit from version.
  version = bazel_version.split(" ", 1)[0]

  # Split into (release, date) parts and only return the release
  # as a tuple of integers.
  parts = version.split('-', 1)

  # Turn "release" into a tuple of strings
  version_tuple = ()
  for number in parts[0].split('.'):
    version_tuple += (str(number),)
  return version_tuple

# Check that a minimum version of bazel is being used.
def check_min_bazel_version(bazel_version):
    if "bazel_version" in dir(native) and native.bazel_version:
        current_bazel_version = _parse_bazel_version(native.bazel_version)
        minimum_bazel_version = _parse_bazel_version(bazel_version)
        if minimum_bazel_version > current_bazel_version:
            fail("\nCurrent Bazel version is {}, expected at least {}\n".format(
                native.bazel_version, bazel_version))
# End: From tensorflow/workspace.bzl
