# DEJAVIEW(1)

## NAME

dejaview - capture traces

## DESCRIPTION

This section describes how to use the `dejaview` commandline binary to capture
traces. Examples are given in terms of an Android device connected over ADB.

`dejaview` has two modes for configuring the tracing session (i.e. what and how
to collect):

__lightweight mode__
: all config options are supplied as commandline flags,
  but the available data sources are restricted to ftrace and atrace. This mode
  is similar to
  [`systrace`](https://developer.android.com/topic/performance/tracing/command-line).

__normal mode__
: the configuration is specified in a protocol buffer. This allows for full
  customisation of collected traces.


## GENERAL OPTIONS

The following table lists the available options when using `dejaview` in either
mode.

`-d`, `--background`
:    DejaView immediately exits the command-line interface and continues
     recording your trace in background.

`-o`, `--out` _OUT_FILE_
:    Specifies the desired path to the output trace file, or `-` for stdout.
     `dejaview` writes the output to the file described in the flags above.
     The output format compiles with the format defined in
     [AOSP `trace.proto`](/protos/dejaview/trace/trace.proto).

`--dropbox` _TAG_
:    Uploads your trace via the
     [DropBoxManager API](https://developer.android.com/reference/android/os/DropBoxManager.html)
     using the tag you specify. Android only.

`--no-guardrails`
:     Disables protections against excessive resource usage when enabling the
      `--dropbox` flag during testing.


`--reset-guardrails`
:     Resets the persistent state of the guardrails and exits (for testing).

`--query`
:     Queries the service state and prints it as human-readable text.

`--query-raw`
:     Similar to `--query`, but prints raw proto-encoded bytes of
      `tracing_service_state.proto`.

`-h`,  `--help`
:     Prints out help text for the `dejaview` tool.


## SIMPLE MODE

For ease of use, the `dejaview` command includes support for a subset of
configurations via command line arguments. On-device, these
configurations behave equivalently to the same configurations provided
by a *CONFIG_FILE* (see below).

The general syntax for using `dejaview` in *simple mode* is as follows:

```
 adb shell dejaview [ --time TIMESPEC ] [ --buffer SIZE ] [ --size SIZE ]
    [ ATRACE_CAT | FTRACE_GROUP/FTRACE_NAME]...
```


The following table lists the available options when using `dejaview` in
*simple mode*.

`-t`, `--time` _TIME[s|m|h]_
:    Specifies the trace duration in seconds, minutes, or hours.
     For example, `--time 1m` specifies a trace duration of 1 minute.
     The default duration is 10 seconds.

`-b`, `--buffer` _SIZE[mb|gb]_
:    Specifies the ring buffer size in megabytes (mb) or gigabytes (gb).
     The default parameter is `--buffer 32mb`.

`-s`, `--size` _SIZE[mb|gb]_
:    Specifies the max file size in megabytes (mb) or gigabytes (gb).
     By default `dejaview` uses only in-memory ring-buffer.


This is followed by a list of event specifiers:

`ATRACE_CAT`
:    Specifies the atrace categories you want to record a trace for.
     For example, the following command traces Window Manager using atrace:
     `adb shell dejaview --out FILE wm`. To record other categories, see the
     [list of atrace categories](https://cs.android.com/android/platform/superproject/main/+/main:frameworks/native/cmds/atrace/atrace.cpp).
     Note: Available categories are Android version dependent.

`FTRACE_GROUP/FTRACE_NAME`
:    Specifies the ftrace events you want to record a trace for.
     For example, the following command traces sched/sched_switch events:
     `adb shell dejaview --out FILE sched/sched_switch`


## NORMAL MODE

The general syntax for using `dejaview` in *normal mode* is as follows:

```
 adb shell dejaview [ --txt ] --config CONFIG_FILE
```

The following table lists the available options when using `dejaview` in
*normal* mode.

`-c`, `--config` _CONFIG_FILE_
:    Specifies the path to a configuration file. In normal mode, some
     configurations may be encoded in a configuration protocol buffer.
     This file must comply with the protocol buffer schema defined in AOSP
     [`trace_config.proto`](/protos/dejaview/config/trace_config.proto).
     You select and configure the data sources using the DataSourceConfig member
     of the TraceConfig, as defined in AOSP
     [`data_source_config.proto`](/protos/dejaview/config/data_source_config.proto).

`--txt`
:    Instructs `dejaview` to parse the config file as pbtxt. This flag is
     experimental, and it's not recommended that you enable it for production.
