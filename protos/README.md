# DejaView Protobuf definitions

There are three groups of protos in the respective dejaview/X directories:

`trace/`: This directory contains all protos that are output-ed by DejaView
when capturing a trace.

`config/`: This directory contains the configuration passed as input to
various parts of DejaView, such as:
  * (i) the whole trace config that the Consumer sends to the DejaView trace
    daemon.
  * (ii) the per-data-source config that the trace daemon relays to each
    Producer.
  * (iii) the per-data-source descriptor that each Producer advertises to the
    trace daemon when registering.

`ipc/`: This directory contains the definition of the IPC surface between
the DejaView components (Producer, trace daemon and Consumer(s)). This is
relevant only in some DejaView build configuration (i.e. in Android but not
in Chrome).
