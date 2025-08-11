// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import protos from '../gen/protos';

// Aliases protos to avoid the super nested namespaces.
// See https://www.typescriptlang.org/docs/handbook/namespaces.html#aliases
import BufferConfig = protos.dejaview.protos.TraceConfig.BufferConfig;
import ComputeMetricArgs = protos.dejaview.protos.ComputeMetricArgs;
import ComputeMetricResult = protos.dejaview.protos.ComputeMetricResult;
import ConsumerPort = protos.dejaview.protos.ConsumerPort;
import DataSourceConfig = protos.dejaview.protos.DataSourceConfig;
import DataSourceDescriptor = protos.dejaview.protos.DataSourceDescriptor;
import DisableAndReadMetatraceResult = protos.dejaview.protos.DisableAndReadMetatraceResult;
import DisableTracingRequest = protos.dejaview.protos.DisableTracingRequest;
import DisableTracingResponse = protos.dejaview.protos.DisableTracingResponse;
import EnableMetatraceArgs = protos.dejaview.protos.EnableMetatraceArgs;
import EnableTracingRequest = protos.dejaview.protos.EnableTracingRequest;
import EnableTracingResponse = protos.dejaview.protos.EnableTracingResponse;
import FreeBuffersRequest = protos.dejaview.protos.FreeBuffersRequest;
import FreeBuffersResponse = protos.dejaview.protos.FreeBuffersResponse;
import GetTraceStatsRequest = protos.dejaview.protos.GetTraceStatsRequest;
import GetTraceStatsResponse = protos.dejaview.protos.GetTraceStatsResponse;
import IBufferConfig = protos.dejaview.protos.TraceConfig.IBufferConfig;
import IBufferStats = protos.dejaview.protos.TraceStats.IBufferStats;
import IDisableTracingResponse = protos.dejaview.protos.IDisableTracingResponse;
import IEnableTracingResponse = protos.dejaview.protos.IEnableTracingResponse;
import IFreeBuffersResponse = protos.dejaview.protos.IFreeBuffersResponse;
import IGetTraceStatsResponse = protos.dejaview.protos.IGetTraceStatsResponse;
import IMethodInfo = protos.dejaview.protos.IPCFrame.BindServiceReply.IMethodInfo;
import IPCFrame = protos.dejaview.protos.IPCFrame;
import IProcessStatsConfig = protos.dejaview.protos.IProcessStatsConfig;
import IReadBuffersResponse = protos.dejaview.protos.IReadBuffersResponse;
import ISlice = protos.dejaview.protos.ReadBuffersResponse.ISlice;
import ISysStatsConfig = protos.dejaview.protos.ISysStatsConfig;
import ITraceConfig = protos.dejaview.protos.ITraceConfig;
import ITraceStats = protos.dejaview.protos.ITraceStats;
import MeminfoCounters = protos.dejaview.protos.MeminfoCounters;
import MetatraceCategories = protos.dejaview.protos.MetatraceCategories;
import DejaViewMetatrace = protos.dejaview.protos.DejaViewMetatrace;
import ProcessStatsConfig = protos.dejaview.protos.ProcessStatsConfig;
import QueryArgs = protos.dejaview.protos.QueryArgs;
import QueryResult = protos.dejaview.protos.QueryResult;
import QueryServiceStateRequest = protos.dejaview.protos.QueryServiceStateRequest;
import QueryServiceStateResponse = protos.dejaview.protos.QueryServiceStateResponse;
import ReadBuffersRequest = protos.dejaview.protos.ReadBuffersRequest;
import ReadBuffersResponse = protos.dejaview.protos.ReadBuffersResponse;
import RegisterSqlPackageArgs = protos.dejaview.protos.RegisterSqlPackageArgs;
import RegisterSqlPackageResult = protos.dejaview.protos.RegisterSqlPackageResult;
import ResetTraceProcessorArgs = protos.dejaview.protos.ResetTraceProcessorArgs;
import StatCounters = protos.dejaview.protos.SysStatsConfig.StatCounters;
import StatusResult = protos.dejaview.protos.StatusResult;
import SysStatsConfig = protos.dejaview.protos.SysStatsConfig;
import TraceConfig = protos.dejaview.protos.TraceConfig;
import TraceProcessorApiVersion = protos.dejaview.protos.TraceProcessorApiVersion;
import TraceProcessorRpc = protos.dejaview.protos.TraceProcessorRpc;
import TraceProcessorRpcStream = protos.dejaview.protos.TraceProcessorRpcStream;
import TrackEventConfig = protos.dejaview.protos.TrackEventConfig;
import VmstatCounters = protos.dejaview.protos.VmstatCounters;

export {
  BufferConfig,
  ComputeMetricArgs,
  ComputeMetricResult,
  ConsumerPort,
  DataSourceConfig,
  DataSourceDescriptor,
  DisableAndReadMetatraceResult,
  DisableTracingRequest,
  DisableTracingResponse,
  EnableMetatraceArgs,
  EnableTracingRequest,
  EnableTracingResponse,
  FreeBuffersRequest,
  FreeBuffersResponse,
  GetTraceStatsRequest,
  GetTraceStatsResponse,
  IBufferConfig,
  IBufferStats,
  IDisableTracingResponse,
  IEnableTracingResponse,
  IFreeBuffersResponse,
  IGetTraceStatsResponse,
  IMethodInfo,
  IPCFrame,
  IProcessStatsConfig,
  IReadBuffersResponse,
  ISlice,
  ISysStatsConfig,
  ITraceConfig,
  ITraceStats,
  MeminfoCounters,
  MetatraceCategories,
  DejaViewMetatrace,
  ProcessStatsConfig,
  QueryArgs,
  QueryResult,
  QueryServiceStateRequest,
  QueryServiceStateResponse,
  ReadBuffersRequest,
  ReadBuffersResponse,
  RegisterSqlPackageArgs,
  RegisterSqlPackageResult,
  ResetTraceProcessorArgs,
  StatCounters,
  StatusResult,
  SysStatsConfig,
  TraceConfig,
  TraceProcessorApiVersion,
  TraceProcessorRpc,
  TraceProcessorRpcStream,
  TrackEventConfig,
  VmstatCounters,
};
