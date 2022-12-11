load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

cc_library(
  name = "reprocpp",
  hdrs = glob(["include/reproc++/*.hpp", "include/reproc++/detail/*.hpp"]),
  srcs = glob(["src/*.cpp"]),
  includes = ["include"],
  visibility = ["//visibility:public"],
  deps = [
    "//reproc:reproc",
  ],
  copts = [
    "-std=c++17",
  ],
  linkstatic = 1,
  alwayslink = 1,
)
