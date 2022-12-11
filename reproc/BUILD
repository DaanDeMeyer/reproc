load("@rules_cc//cc:defs.bzl", "cc_library")


common_files = [
 "src/drain.c",
 "src/options.c",
 "src/redirect.c",
 "src/reproc.c",
 "src/run.c",
 "src/strv.c",
]


cc_library(
  name = "reproc",
  hdrs = glob(["include/reproc/*.h", "src/*.h"]),
  srcs = common_files +
    select({
      "@bazel_tools//src/conditions:windows": glob(["src/*.windows.c"]),
      "//conditions:default": glob(["src/*.posix.c"]),
    }),
  includes = ["include"],
  visibility = ["//visibility:public"],
  linkopts = select({
    "@bazel_tools//src/conditions:windows": [
      "-lws2_32",
    ],
    "//conditions:default": ["-lrt", "-lpthread"],
  }),
  copts = ["-std=c99"],
  linkstatic = 1,
  alwayslink = 1,
)
