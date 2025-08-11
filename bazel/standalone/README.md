# DejaView standalone Bazel config

This directory is only used in standalone builds.
The WORKSPACE aliases this directory to @dejaview_cfg.

Bazel-based embedders are supposed to:

### 1. Have a (modified) copy of dejaview_cfg.bzl in their repo

```
myproject/
  build/
    dejaview_overrides/
      dejaview_cfg.bzl
```

### 2. Have a repository rule that maps the directory to @dejaview_cfg

E.g in myproject/WORKSPACE
```
local_repository(
    name = "dejaview_cfg",
    path = "build/dejaview_overrides",
)
```
