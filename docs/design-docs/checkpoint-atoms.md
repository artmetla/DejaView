# Statsd Checkpoint Atoms
## Tracing

This diagram gives the atoms and the state transitions between when tracing/
All atoms above log the UUID of the trace;
`DEJAVIEW_TRACED_TRIGGER_STOP_TRACING` is special as it *also* logs the trigger
name which caused trace finalization.

NOTE: dotted lines indicate these transitions only happen in background
configs; transitions with solid lines happen in both background and
non-background cases.

NOTE: for background traces, *either* start triggers or stop triggers are
supported; both cannot happen for the same trace.

```mermaid
graph TD;
    DEJAVIEW_CMD_TRACE_BEGIN-->DEJAVIEW_CMD_ON_CONNECT;
    DEJAVIEW_CMD_BACKGROUND_TRACE_BEGIN-.->DEJAVIEW_CMD_ON_CONNECT
    DEJAVIEW_CMD_ON_CONNECT-->DEJAVIEW_TRACED_ENABLE_TRACING
    DEJAVIEW_TRACED_ENABLE_TRACING-->DEJAVIEW_TRACED_START_TRACING
    DEJAVIEW_TRACED_ENABLE_TRACING-.->|start trigger background traces only|DEJAVIEW_TRACED_TRIGGER_START_TRACING
    DEJAVIEW_TRACED_TRIGGER_START_TRACING-.->DEJAVIEW_TRACED_START_TRACING
    DEJAVIEW_TRACED_START_TRACING-.->|stop trigger background traces only|DEJAVIEW_TRACED_TRIGGER_STOP_TRACING
    DEJAVIEW_TRACED_TRIGGER_STOP_TRACING-.->DEJAVIEW_TRACED_DISABLE_TRACING
    DEJAVIEW_TRACED_START_TRACING-->DEJAVIEW_TRACED_DISABLE_TRACING
    DEJAVIEW_TRACED_DISABLE_TRACING-->DEJAVIEW_TRACED_NOTIFY_TRACING_DISABLED
    DEJAVIEW_TRACED_NOTIFY_TRACING_DISABLED-->DEJAVIEW_CMD_ON_TRACING_DISABLED
    DEJAVIEW_CMD_ON_TRACING_DISABLED-->DEJAVIEW_CMD_FINALIZE_TRACE_AND_EXIT
    DEJAVIEW_CMD_FINALIZE_TRACE_AND_EXIT-->DEJAVIEW_CMD_UPLOAD_INCIDENT
    DEJAVIEW_CMD_FINALIZE_TRACE_AND_EXIT-.->|only if no trigger happened|DEJAVIEW_CMD_NOT_UPLOADING_EMPTY_TRACE
```

## Triggers

This diagram gives the atoms which can trigger finalization of a trace. 
These atoms will not be reported individually but instead aggregated by trigger name
and reported as a count.

```mermaid
graph TD;
    DEJAVIEW_CMD_TRIGGER
    DEJAVIEW_TRIGGER_DEJAVIEW_TRIGGER
```

