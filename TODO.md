# TODO

QEMU:

- add the ability to take snapshots from the plugin

QEMU plugin:

- add a during=process argument to the qemu plugin to restrict to a process name lifetime
- implement an "ignore_below=apply_returns"
- log the qemu serial output in the trace (see AndroidLogs for reference)
- introspect the current process's executable path and lookup debug info from disk

UI:

- visualize serial logs so that a click on a character brings back to the serial write
- add the ability to seek-to an instruction number and add a snapshot there

Global:

- fix the buffered_frame_deserializer_fuzzer error
