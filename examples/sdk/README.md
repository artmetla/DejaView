# DejaView SDK example project

This directory contains an example project using the [DejaView
SDK](https://perfetto.dev/docs/instrumentation/tracing-sdk). It demonstrates
how to instrument your application with track events to give more context in
developing, debugging and performance analysis.

Dependencies:

- [CMake](https://cmake.org/)
- C++17

## Building

First, check out the latest DejaView release:

```bash
git clone https://android.googlesource.com/platform/external/perfetto -b v47.0
```

Then, build using CMake:

```bash
cd dejaview/examples/sdk
cmake -B build
cmake --build build
```

Note: If amalgamated source files are not present, generate them using
`cd dejaview ; tools/gen_amalgamated --output sdk/dejaview`.
[Learn more](https://perfetto.dev/docs/contributing/sdk-releasing#building-and-tagging-the-release)
at the release section.

## Track event example

The [basic example](example.cc) shows how to instrument an app with track
events. Run it with:

```bash
build/example
```

The program will create a trace file in `example.dejaview-trace`, which can be
directly opened in the [DejaView UI](https://ui.perfetto.dev). The result
should look like this:

![Example trace loaded in the DejaView UI](
  example.png "Example trace loaded in the DejaView UI")

## System-wide example

While the above example only records events from the program itself, with
DejaView it's also possible to combine app trace events with system-wide
profiling data (e.g., ftrace on Linux). The repository has a [second
example](example_system_wide.cc) which demonstrates this on Android.

Requirements:
- [Android NDK](https://developer.android.com/ndk)
- A device running Android Pie or newer

> Tip: It's also possible to sideload DejaView on pre-Pie Android devices.
> See the [build
> instructions](https://perfetto.dev/docs/contributing/build-instructions).

To build:

```bash
export NDK=/path/to/ndk
cmake -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-21 \
      -DANDROID_LD=lld \
      -DCMAKE_BUILD_TYPE=Release \
      -B build_android
cmake --build build_android
```

Next, plug in an Android device into a USB port, download the example and run
it while simultaneously recording a trace using the `dejaview` command line
tool:

```bash
adb push build_android/example_system_wide ../system_wide_trace_cfg.pbtxt \
         /data/local/tmp/
adb shell "\
    cd /data/local/tmp; \
    rm -f /data/misc/dejaview-traces/example_system_wide.dejaview-trace; \
    cat system_wide_trace_cfg.pbtxt | \
        dejaview --config - --txt --background \
                 -o
                 /data/misc/dejaview-traces/example_system_wide.dejaview-trace; \
    ./example_system_wide"
```

Finally, retrieve the resulting trace:

```bash
adb pull /data/misc/dejaview-traces/example_system_wide.dejaview-trace
```

When opened in the DejaView UI, the trace now shows additional contextual
information such as CPU frequencies and kernel scheduler information.

![Example system wide-trace loaded in the DejaView UI](
  example_system_wide.png "Example system-wide trace in the DejaView UI")

> Tip: You can generate a new trace config with additional data sources using
> the [DejaView UI](https://ui.perfetto.dev/#!/record) and replace
> `system_wide_trace_cfg.pbtxt` with the [generated config](
> https://ui.perfetto.dev/#!/record/instructions).

## Custom data source example

The [final example](example_custom_data_source.cc) shows how to use an
application defined data source to emit custom, strongly typed data into a
trace. Run it with:

```bash
build/example_custom_data_source
```

The program generates a trace file in `example_custom_data_source.dejaview-trace`,
which we can examine using DejaView's `traceconv` tool to show the trace
packet written by the custom data source:

```bash
traceconv text example_custom_data_source.dejaview-trace
...
packet {
  trusted_uid: 0
  timestamp: 42
  trusted_packet_sequence_id: 2
  previous_packet_dropped: true
  for_testing {
    str: "Hello world!"
  }
}
...
```
