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

# docker:stable is an Alpine-based distro.
FROM docker:stable

RUN apk update && apk add python3 py-pip sudo tini
RUN pip3 install oauth2client httplib2 google-auth google-cloud requests;


# Unfortunately Docker doesn't allow to copy a file from ../. So we copy instead
# the config files into tmp/ from the Makefile that runs docker build.
COPY tmp/config.py /home/dejaview/config.py
COPY tmp/common_utils.py /home/dejaview/common_utils.py
COPY artifacts_uploader.py /home/dejaview/
COPY perf_metrics_uploader.py /home/dejaview/
COPY run_job.py /home/dejaview/
COPY worker.py /home/dejaview/

# Allow the worker to spawn new docker containers (for the jobs' sandboxes).
# This makes the worker container highly priviledged (effectiveely can run  any
# commands on the GCE vm). The worker container is trusted and must never run
# code from a tryjob (which instead is run into the sandbox containers).
RUN set -e; \
    echo 'root ALL=(ALL) ALL' /etc/sudoers; \
    echo 'dejaview ALL=(ALL) NOPASSWD: ALL' >> /etc/sudoers; \
    addgroup -S --gid 1337 dejaview; \
    adduser -S --uid 1337 -h /home/dejaview dejaview dejaview; \
    chown dejaview.dejaview -R /home/dejaview; \
    chmod -R 755 /home/dejaview;

USER dejaview:dejaview
WORKDIR /home/dejaview

ENTRYPOINT [ "tini", "--" ]
CMD [ "python3", "/home/dejaview/worker.py" ]
