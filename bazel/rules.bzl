# Copyright (C) 2019 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@dejaview_cfg//:dejaview_cfg.bzl", "DEJAVIEW_CONFIG")
load("@dejaview//bazel:proto_gen.bzl", "proto_descriptor_gen", "proto_gen")

# +----------------------------------------------------------------------------+
# | Base C++ rules.                                                            |
# +----------------------------------------------------------------------------+

def default_cc_args():
    return {
        "deps": DEJAVIEW_CONFIG.deps.build_config,
        "copts": DEJAVIEW_CONFIG.default_copts + [
            "-Wno-pragma-system-header-outside-header",
        ],
        "includes": ["include"],
        "linkopts": select({
            "@dejaview//bazel:os_linux": ["-ldl", "-lrt", "-lpthread"],
            "@dejaview//bazel:os_osx": [],
            "@dejaview//bazel:os_windows": ["ws2_32.lib"],
            "//conditions:default": ["-ldl"],
        }),
    }

def dejaview_build_config_cc_library(**kwargs):
    if not _rule_override("cc_library", **kwargs):
        native.cc_library(**kwargs)

def dejaview_filegroup(**kwargs):
    if not _rule_override("filegroup", **kwargs):
        native.filegroup(**kwargs)

def dejaview_genrule(**kwargs):
    if not _rule_override("genrule", **kwargs):
        native.genrule(**kwargs)

def dejaview_cc_library(**kwargs):
    args = _merge_dicts(default_cc_args(), kwargs)
    if not _rule_override("cc_library", **args):
        native.cc_library(**args)

def dejaview_cc_binary(**kwargs):
    args = _merge_dicts(default_cc_args(), kwargs)
    if not _rule_override("cc_binary", **args):
        native.cc_binary(**args)

def dejaview_py_binary(**kwargs):
    if not _rule_override("py_binary", **kwargs):
        native.py_binary(**kwargs)

def dejaview_py_library(**kwargs):
    if not _rule_override("py_library", **kwargs):
        native.py_library(**kwargs)

# +----------------------------------------------------------------------------+
# | Proto-related rules                                                        |
# +----------------------------------------------------------------------------+

def dejaview_proto_library(**kwargs):
    if not _rule_override("proto_library", **kwargs):
        native.proto_library(**kwargs)

def dejaview_cc_proto_library(**kwargs):
    if not _rule_override("cc_proto_library", **kwargs):
        native.cc_proto_library(**kwargs)

def dejaview_java_proto_library(**kwargs):
    if not _rule_override("java_proto_library", **kwargs):
        native.java_proto_library(**kwargs)

def dejaview_java_lite_proto_library(**kwargs):
    if not _rule_override("java_lite_proto_library", **kwargs):
        native.java_lite_proto_library(**kwargs)

# Unlike the other rules, this is an noop by default because Bazel does not
# support Go proto libraries.
def dejaview_go_proto_library(**kwargs):
    _rule_override("go_proto_library", **kwargs)

# Unlike the other rules, this is an noop by default because Bazel does not
# support Python proto libraries.
def dejaview_py_proto_library(**kwargs):
    _rule_override("py_proto_library", **kwargs)

# Unlike the other rules, this is an noop by default because Bazel does not
# support Javascript/Typescript proto libraries.
def dejaview_jspb_proto_library(**kwargs):
    _rule_override("jspb_proto_library", **kwargs)

# +----------------------------------------------------------------------------+
# | Misc rules.                                                                |
# +----------------------------------------------------------------------------+

# Generates .pbzero.{cc,h} from .proto(s). We deliberately do NOT generate
# conventional .pb.{cc,h} from here as protozero gen sources do not have any
# dependency on libprotobuf.
def dejaview_cc_protozero_library(name, deps, **kwargs):
    if _rule_override(
        "cc_protozero_library",
        name = name,
        deps = deps,
        **kwargs
    ):
        return

    # A dejaview_cc_protozero_library has two types of dependencies:
    # 1. Exactly one dependency on a proto_library target. This defines the
    #    .proto sources for the target
    # 2. Zero or more deps on other dejaview_cc_protozero_library targets. This
    #    to deal with the case of foo.proto including common.proto from another
    #    target.
    _proto_deps = [d for d in deps if d.endswith("_protos")]
    _cc_deps = [d for d in deps if d not in _proto_deps]
    if len(_proto_deps) != 1:
        fail("Too many proto deps for target %s" % name)

    args = {
        'name': name + "_src",
        'deps': _proto_deps,
        'suffix': "pbzero",
        'plugin': DEJAVIEW_CONFIG.root + ":protozero_plugin",
        'wrapper_namespace': "pbzero",
        'protoc': DEJAVIEW_CONFIG.deps.protoc[0],
        'root': DEJAVIEW_CONFIG.root,
    }
    if not _rule_override("proto_gen", **args):
        proto_gen(**args)

    dejaview_filegroup(
        name = name + "_h",
        srcs = [":" + name + "_src"],
        output_group = "h",
    )

    dejaview_cc_library(
        name = name,
        srcs = [":" + name + "_src"],
        hdrs = [":" + name + "_h"],
        deps = [DEJAVIEW_CONFIG.root + ":protozero"] + _cc_deps,
        **kwargs
    )

# Generates .ipc.{cc,h} and .pb.{cc.h} from .proto(s). The IPC sources depend
# on .pb.h so we need to generate also the standard protobuf sources here.
def dejaview_cc_ipc_library(name, deps, **kwargs):
    if _rule_override("cc_ipc_library", name = name, deps = deps, **kwargs):
        return

    # A dejaview_cc_ipc_library has two types of dependencies:
    # 1. Exactly one dependency on a proto_library target. This defines the
    #    .proto sources for the target
    # 2. Zero or more deps on other dejaview_cc_protocpp_library targets. This
    #    to deal with the case of foo.proto including common.proto from another
    #    target.
    _proto_deps = [d for d in deps if d.endswith("_protos")]
    _cc_deps = [d for d in deps if d not in _proto_deps]
    if len(_proto_deps) != 1:
        fail("Too many proto deps for target %s" % name)

    # Generates .ipc.{cc,h}.
    args = {
        'name': name + "_src",
        'deps': _proto_deps,
        'suffix': "ipc",
        'plugin': DEJAVIEW_CONFIG.root + ":ipc_plugin",
        'wrapper_namespace': "gen",
        'protoc': DEJAVIEW_CONFIG.deps.protoc[0],
        'root': DEJAVIEW_CONFIG.root,
    }
    if not _rule_override("proto_gen", **args):
        proto_gen(**args)

    dejaview_filegroup(
        name = name + "_h",
        srcs = [":" + name + "_src"],
        output_group = "h",
    )

    dejaview_cc_library(
        name = name,
        srcs = [":" + name + "_src"],
        hdrs = [":" + name + "_h"],
        deps = [
            # Generated .ipc.{cc,h} depend on this and protozero.
            DEJAVIEW_CONFIG.root + ":dejaview_ipc",
            DEJAVIEW_CONFIG.root + ":protozero",
        ] + _cc_deps,
        **kwargs
    )

# Generates .gen.{cc,h} from .proto(s).
def dejaview_cc_protocpp_library(name, deps, **kwargs):
    if _rule_override(
        "cc_protocpp_library",
        name = name,
        deps = deps,
        **kwargs
    ):
        return

    # A dejaview_cc_protocpp_library has two types of dependencies:
    # 1. Exactly one dependency on a proto_library target. This defines the
    #    .proto sources for the target
    # 2. Zero or more deps on other dejaview_cc_protocpp_library targets. This
    #    to deal with the case of foo.proto including common.proto from another
    #    target.
    _proto_deps = [d for d in deps if d.endswith("_protos")]
    _cc_deps = [d for d in deps if d not in _proto_deps]
    if len(_proto_deps) != 1:
        fail("Too many proto deps for target %s" % name)

    args = {
        'name': name + "_gen",
        'deps': _proto_deps,
        'suffix': "gen",
        'plugin': DEJAVIEW_CONFIG.root + ":cppgen_plugin",
        'wrapper_namespace': "gen",
        'protoc': DEJAVIEW_CONFIG.deps.protoc[0],
        'root': DEJAVIEW_CONFIG.root,
    }
    if not _rule_override("proto_gen", **args):
        proto_gen(**args)

    dejaview_filegroup(
        name = name + "_gen_h",
        srcs = [":" + name + "_gen"],
        output_group = "h",
    )

    # The headers from the gen plugin have implicit dependencies
    # on each other so will fail when compiled independently. Use
    # textual_hdrs to indicate this to Bazel.
    dejaview_cc_library(
        name = name,
        srcs = [":" + name + "_gen"],
        textual_hdrs = [":" + name + "_gen_h"],
        deps = [
            DEJAVIEW_CONFIG.root + ":protozero",
        ] + _cc_deps,
        **kwargs
    )

def dejaview_proto_descriptor(name, deps, outs, **kwargs):
    args = {
        'name': name,
        'deps': deps,
        'outs': outs,
    }
    if not _rule_override("proto_descriptor_gen", **args):
        proto_descriptor_gen(**args)

# Generator .descriptor.h from protos
def dejaview_cc_proto_descriptor(name, deps, outs, **kwargs):
    cmd = [
        "$(location gen_cc_proto_descriptor_py)",
        "--cpp_out=$@",
        "--gen_dir=$(GENDIR)",
        "$<"
    ]
    dejaview_genrule(
        name = name + "_gen",
        cmd = " ".join(cmd),
        tools = [
            ":gen_cc_proto_descriptor_py",
        ],
        srcs = deps,
        outs = outs,
    )

    dejaview_cc_library(
        name = name,
        hdrs = [":" + name + "_gen"],
        **kwargs
    )

def dejaview_cc_amalgamated_sql(name, deps, outs, namespace, **kwargs):
    if DEJAVIEW_CONFIG.root[:2] != "//":
        fail("Expected DEJAVIEW_CONFIG.root to start with //")

    genrule_tool = kwargs.pop("genrule_tool", ":gen_amalgamated_sql_py")
    cmd = [
        "$(location " + genrule_tool + ")",
        "--namespace",
        namespace,
        "--cpp-out=$@",
        "$(SRCS)",
    ]

    root_dir = kwargs.pop("root_dir", None)
    if root_dir:
        cmd += [
            "--root-dir",
            root_dir,
        ]

    dejaview_genrule(
        name = name + "_gen",
        cmd = " ".join(cmd),
        tools = [
            genrule_tool,
        ],
        srcs = deps,
        outs = outs,
    )
    dejaview_cc_library(
        name = name,
        hdrs = [":" + name + "_gen"],
        **kwargs
    )

def dejaview_cc_tp_tables(name, srcs, outs, deps = [], **kwargs):
    if DEJAVIEW_CONFIG.root[:2] != "//":
        fail("Expected DEJAVIEW_CONFIG.root to start with //")

    if DEJAVIEW_CONFIG.root == "//":
        python_path = DEJAVIEW_CONFIG.root + "python"
    else:
        python_path = DEJAVIEW_CONFIG.root + "/python"

    dejaview_py_library(
        name = name + "_lib",
        deps = [
            python_path + ":trace_processor_table_generator",
        ],
        srcs = srcs,
    )

    dejaview_py_binary(
        name = name + "_tool",
        deps = [
            ":" + name + "_lib",
            python_path + ":trace_processor_table_generator",
        ] + [d + "_lib" for d in deps],
        srcs = [
            "tools/gen_tp_table_headers.py",
        ],
        main = "tools/gen_tp_table_headers.py",
        python_version = "PY3",
    )

    cmd = ["$(location " + name + "_tool)"]
    cmd += ["--gen-dir", "$(RULEDIR)"]
    cmd += ["--inputs", "$(SRCS)"]
    if DEJAVIEW_CONFIG.root != "//":
        cmd += ["--import-prefix", DEJAVIEW_CONFIG.root[2:]]
        cmd += ["--relative-input-dir", DEJAVIEW_CONFIG.root[2:]]

    dejaview_genrule(
        name = name + "_gen",
        cmd = " ".join(cmd),
        tools = [
            ":" + name + "_tool",
        ],
        srcs = srcs,
        outs = outs,
    )

    dejaview_filegroup(
        name = name,
        srcs = [":" + name + "_gen"],
        **kwargs,
    )

# +----------------------------------------------------------------------------+
# | Misc utility functions                                                     |
# +----------------------------------------------------------------------------+

def _rule_override(rule_name, **kwargs):
    overrides = getattr(DEJAVIEW_CONFIG, "rule_overrides", struct())
    overridden_rule = getattr(overrides, rule_name, None)
    if overridden_rule:
        overridden_rule(**kwargs)
        return True
    return False

def _merge_dicts(*args):
    res = {}
    for arg in args:
        for k, v in arg.items():
            if type(v) == "string" or type(v) == "bool":
                res[k] = v
            elif type(v) == "list" or type(v) == "select":
                res[k] = res.get(k, []) + v
            else:
                fail("key type not supported: " + type(v))
    return res
