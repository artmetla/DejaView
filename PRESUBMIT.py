# Copyright (C) 2017 The Android Open Source Project
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

from __future__ import print_function
import itertools
import subprocess
import time

USE_PYTHON3 = True


def RunAndReportIfLong(func, *args, **kargs):
  start = time.time()
  results = func(*args, **kargs)
  end = time.time()
  limit = 3.0  # seconds
  name = func.__name__
  runtime = end - start
  if runtime > limit:
    print("{} took >{:.2}s ({:.2}s)".format(name, limit, runtime))
  return results


def CheckChange(input, output):
  # There apparently is no way to wrap strings in blueprints, so ignore long
  # lines in them.
  def long_line_sources(x):
    return input.FilterSourceFile(
        x,
        files_to_check='.*',
        files_to_skip=[
            '.*[.]json$',
            '.*[.]sql$',
            '.*[.]out$',
            'test/trace_processor/.*/tests.*$',
            '(.*/)?BUILD$',
            'WORKSPACE',
            '.*/Makefile$',
            '/dejaview_build_flags.h$',
            "infra/luci/.*",
            "^ui/.*\.[jt]s$",  # TS/JS handled by eslint
            "^ui/pnpm-lock.yaml$",
        ])

  results = []
  results += RunAndReportIfLong(input.canned_checks.CheckDoNotSubmit, input,
                                output)
  results += RunAndReportIfLong(input.canned_checks.CheckChangeHasNoTabs, input,
                                output)
  results += RunAndReportIfLong(
      input.canned_checks.CheckLongLines,
      input,
      output,
      80,
      source_file_filter=long_line_sources)
  # TS/JS handled by eslint
  results += RunAndReportIfLong(
      input.canned_checks.CheckPatchFormatted, input, output, check_js=False)
  results += RunAndReportIfLong(input.canned_checks.CheckGNFormatted, input,
                                output)
  results += RunAndReportIfLong(CheckIncludeGuards, input, output)
  results += RunAndReportIfLong(CheckIncludeViolations, input, output)
  results += RunAndReportIfLong(CheckIncludePaths, input, output)
  results += RunAndReportIfLong(CheckProtoComments, input, output)
  results += RunAndReportIfLong(CheckBinaryDescriptors, input, output)
  results += RunAndReportIfLong(CheckMergedTraceConfigProto, input, output)
  results += RunAndReportIfLong(CheckBannedCpp, input, output)
  results += RunAndReportIfLong(CheckBadCppPatterns, input, output)
  results += RunAndReportIfLong(CheckSqlModules, input, output)
  results += RunAndReportIfLong(CheckSqlMetrics, input, output)
  results += RunAndReportIfLong(CheckAmalgamatedPythonTools, input, output)
  results += RunAndReportIfLong(CheckAbsolutePathsInGn, input, output)
  return results


def CheckChangeOnUpload(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckChangeOnCommit(input_api, output_api):
  return CheckChange(input_api, output_api)


def CheckIncludeGuards(input_api, output_api):
  # The script invocation doesn't work on Windows.
  if input_api.is_windows:
    return []

  tool = 'tools/fix_include_guards'

  def file_filter(x):
    return input_api.FilterSourceFile(
        x, files_to_check=['.*[.]cc$', '.*[.]h$', tool])

  if not input_api.AffectedSourceFiles(file_filter):
    return []
  if subprocess.call([tool, '--check-only']):
    return [
        output_api.PresubmitError('Please run ' + tool +
                                  ' to fix include guards.')
    ]
  return []


def CheckBannedCpp(input_api, output_api):
  bad_cpp = [
      (r'\bstd::stoi\b',
       'std::stoi throws exceptions prefer base::StringToInt32()'),
      (r'\bstd::stol\b',
       'std::stoull throws exceptions prefer base::StringToInt32()'),
      (r'\bstd::stoul\b',
       'std::stoull throws exceptions prefer base::StringToUint32()'),
      (r'\bstd::stoll\b',
       'std::stoull throws exceptions prefer base::StringToInt64()'),
      (r'\bstd::stoull\b',
       'std::stoull throws exceptions prefer base::StringToUint64()'),
      (r'\bstd::stof\b',
       'std::stof throws exceptions prefer base::StringToDouble()'),
      (r'\bstd::stod\b',
       'std::stod throws exceptions prefer base::StringToDouble()'),
      (r'\bstd::stold\b',
       'std::stold throws exceptions prefer base::StringToDouble()'),
      (r'\bstrncpy\b',
       'strncpy does not null-terminate if src > dst. Use base::StringCopy'),
      (r'[(=]\s*snprintf\(',
       'snprintf can return > dst_size. Use base::SprintfTrunc'),
      (r'//.*\bDNS\b',
       '// DNS (Do Not Ship) found. Did you mean to remove some testing code?'),
      (r'\bDEJAVIEW_EINTR\(close\(',
       'close(2) must not be retried on EINTR on Linux and other OSes '
       'that we run on, as the fd will be closed.'),
      (r'^#include <inttypes.h>', 'Use <cinttypes> rather than <inttypes.h>. ' +
       'See https://github.com/google/perfetto/issues/146'),
  ]

  def file_filter(x):
    return input_api.FilterSourceFile(x, files_to_check=[r'.*\.h$', r'.*\.cc$'])

  errors = []
  for f in input_api.AffectedSourceFiles(file_filter):
    for line_number, line in f.ChangedContents():
      if input_api.re.search(r'^\s*//', line):
        continue  # Skip comments
      for regex, message in bad_cpp:
        if input_api.re.search(regex, line):
          errors.append(
              output_api.PresubmitError('Banned pattern:\n  {}:{} {}'.format(
                  f.LocalPath(), line_number, message)))
  return errors


def CheckBadCppPatterns(input_api, output_api):
  bad_patterns = [
      (r'.*/tracing_service_impl[.]cc$', r'\btrigger_config\(\)',
       'Use GetTriggerMode(session->config) rather than .trigger_config()'),
  ]
  errors = []
  for file_regex, code_regex, message in bad_patterns:
    filt = lambda x: input_api.FilterSourceFile(x, files_to_check=[file_regex])
    for f in input_api.AffectedSourceFiles(filt):
      for line_number, line in f.ChangedContents():
        if input_api.re.search(r'^\s*//', line):
          continue  # Skip comments
        if input_api.re.search(code_regex, line):
          errors.append(
              output_api.PresubmitError('{}:{} {}'.format(
                  f.LocalPath(), line_number, message)))
  return errors


def CheckIncludeViolations(input_api, output_api):
  # The script invocation doesn't work on Windows.
  if input_api.is_windows:
    return []

  tool = 'tools/check_include_violations'

  def file_filter(x):
    return input_api.FilterSourceFile(
        x, files_to_check=['include/.*[.]h$', tool])

  if not input_api.AffectedSourceFiles(file_filter):
    return []
  if subprocess.call([tool]):
    return [output_api.PresubmitError(tool + ' failed.')]
  return []


def CheckIncludePaths(input_api, output_api):

  def file_filter(x):
    return input_api.FilterSourceFile(x, files_to_check=[r'.*\.h$', r'.*\.cc$'])

  error_lines = []
  for f in input_api.AffectedSourceFiles(file_filter):
    for line_num, line in f.ChangedContents():
      m = input_api.re.search(r'^#include "(.*\.h)"', line)
      if not m:
        continue
      inc_hdr = m.group(1)
      if inc_hdr.startswith('include/dejaview'):
        error_lines.append('  %s:%s: Redundant "include/" in #include path"' %
                           (f.LocalPath(), line_num))
      if '/' not in inc_hdr:
        error_lines.append(
            '  %s:%s: relative #include not allowed, use full path' %
            (f.LocalPath(), line_num))
  return [] if len(error_lines) == 0 else [
      output_api.PresubmitError('Invalid #include paths detected:\n' +
                                '\n'.join(error_lines))
  ]


def CheckMergedTraceConfigProto(input_api, output_api):
  # The script invocation doesn't work on Windows.
  if input_api.is_windows:
    return []

  tool = 'tools/gen_merged_protos'

  def build_file_filter(x):
    return input_api.FilterSourceFile(
        x, files_to_check=['protos/dejaview/.*[.]proto$', tool])

  if not input_api.AffectedSourceFiles(build_file_filter):
    return []
  if subprocess.call([tool, '--check-only']):
    return [
        output_api.PresubmitError(
            'dejaview_config.proto or dejaview_trace.proto is out of ' +
            'date. Please run ' + tool + ' to update it.')
    ]
  return []


def CheckProtoComments(input_api, output_api):
  # The script invocation doesn't work on Windows.
  if input_api.is_windows:
    return []

  tool = 'tools/check_proto_comments'

  def file_filter(x):
    return input_api.FilterSourceFile(
        x, files_to_check=['protos/dejaview/.*[.]proto$', tool])

  if not input_api.AffectedSourceFiles(file_filter):
    return []
  if subprocess.call([tool]):
    return [output_api.PresubmitError(tool + ' failed')]
  return []


def CheckSqlModules(input_api, output_api):
  # The script invocation doesn't work on Windows.
  if input_api.is_windows:
    return []

  tool = 'tools/check_sql_modules.py'

  def file_filter(x):
    return input_api.FilterSourceFile(
        x,
        files_to_check=[
            'src/trace_processor/dejaview_sql/stdlib/.*[.]sql$', tool
        ])

  if not input_api.AffectedSourceFiles(file_filter):
    return []
  if subprocess.call([tool]):
    return [output_api.PresubmitError(tool + ' failed')]
  return []


def CheckSqlMetrics(input_api, output_api):
  # The script invocation doesn't work on Windows.
  if input_api.is_windows:
    return []

  tool = 'tools/check_sql_metrics.py'

  def file_filter(x):
    return input_api.FilterSourceFile(
        x, files_to_check=['src/trace_processor/metrics/.*[.]sql$', tool])

  if not input_api.AffectedSourceFiles(file_filter):
    return []
  if subprocess.call([tool]):
    return [output_api.PresubmitError(tool + ' failed')]
  return []


def CheckAbsolutePathsInGn(input_api, output_api):

  def file_filter(x):
    return input_api.FilterSourceFile(
        x,
        files_to_check=[r'.*\.gni?$'],
        files_to_skip=[
            '^.gn$',
            '^gn/.*',
            '^buildtools/.*',
        ])

  error_lines = []
  for f in input_api.AffectedSourceFiles(file_filter):
    for line_number, line in f.ChangedContents():
      if input_api.re.search(r'(^\s*[#])|([#]\s*nogncheck)', line):
        continue  # Skip comments and '# nogncheck' lines
      if input_api.re.search(r'"//[^"]', line):
        error_lines.append('  %s:%s: %s' %
                           (f.LocalPath(), line_number, line.strip()))

  if len(error_lines) == 0:
    return []
  return [
      output_api.PresubmitError(
          'Use relative paths in GN rather than absolute:\n' +
          '\n'.join(error_lines))
  ]
