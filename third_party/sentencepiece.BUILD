load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_cc//cc:defs.bzl", "cc_proto_library")

filegroup(
  name = "all",
  srcs = glob(["**"]),
  visibility = ["//visibility:public"],
)

proto_library(
  name = "base_proto",
  srcs = ["src/sentencepiece.proto"],
  strip_import_prefix = "src",
)

cc_proto_library(
  name = "base_cc_proto",
  deps = [":base_proto"],
  visibility = ["//visibility:public"],
)

proto_library(
  name = "model_proto",
  srcs = ["src/sentencepiece_model.proto"],
  strip_import_prefix = "src",
)

cc_proto_library(
  name = "model_cc_proto",
  deps = [":model_proto"],
  visibility = ["//visibility:public"],
)

cc_library(
  name = "thirdparty",
  srcs = [],
  hdrs = [
    "third_party/darts_clone/darts.h",
    "third_party/esaxx/esa.hxx",
  ],
)

cc_library(
    name = "libsentencepiece_internal",
    srcs = [
      "src/bpe_model.cc",
      "src/char_model.cc",
      "src/error.cc",
      "src/filesystem.cc",
      "src/model_factory.cc",
      "src/model_interface.cc",
      "src/normalizer.cc",
      "src/sentencepiece_processor.cc",
      "src/unigram_model.cc",
      "src/util.cc",
      "src/word_model.cc",
    ],
    hdrs = [
      "src/bpe_model.h",
      "src/common.h",
      "src/normalizer.h",
      "src/util.h",
      "src/freelist.h",
      "src/filesystem.h",
      "src/init.h",
      "src/sentencepiece_processor.h",
      "src/word_model.h",
      "src/model_factory.h",
      "src/char_model.h",
      "src/model_interface.h",
      "src/testharness.h",
      "src/unigram_model.h",
    ],
    deps = [
      ":base_cc_proto",
      ":model_cc_proto",
      ":thirdparty",
      "@com_google_absl//absl/strings:strings",
      "@com_google_absl//absl/flags:flag",
      "@com_google_absl//absl/flags:parse",
      "@com_google_absl//absl/container:flat_hash_map",
    ],
    strip_include_prefix = "src",
)

cc_library(
  name = "libsentencepiece",
  srcs = [],
  hdrs = [
    "src/sentencepiece_processor.h",
  ],
  strip_include_prefix = "src",
  include_prefix = "sentencepiece",
  deps = [":libsentencepiece_internal"],
  visibility = ["//visibility:public"],
)
