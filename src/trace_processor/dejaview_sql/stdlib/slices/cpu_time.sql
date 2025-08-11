--
-- Copyright 2024 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.

INCLUDE DEJAVIEW MODULE intervals.intersect;
INCLUDE DEJAVIEW MODULE linux.cpu.utilization.slice;

-- Time each thread slice spent running on CPU.
-- Requires scheduling data to be available in the trace.
CREATE DEJAVIEW TABLE thread_slice_cpu_time(
  -- Id of a slice. Alias of `slice.id`.
  id INT,
  -- Name of the slice.
  name STRING,
  -- Id of the thread the slice is running on. Alias of `thread.id`.
  utid INT,
  -- Name of the thread.
  thread_name STRING,
  -- Id of the process the slice is running on. Alias of `process.id`.
  upid INT,
  -- Name of the process.
  process_name STRING,
  -- Duration of the time the slice was running.
  cpu_time INT) AS
SELECT
id_0 AS id,
name,
ts.utid,
thread_name,
upid,
process_name,
SUM(ii.dur) AS cpu_time
FROM _interval_intersect!((
  (SELECT * FROM thread_slice WHERE utid > 0 AND dur > 0),
  (SELECT * FROM sched WHERE dur > 0)
  ), (utid)) ii
JOIN thread_slice ts ON ts.id = ii.id_0
GROUP BY id
ORDER BY id;
