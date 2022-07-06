#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -vex

. ./updatebot-version.sh # Get UPDATEBOT_REVISION

# If you edit this, be sure to edit fetch/updatebot.yml
export SQLPROXY_REVISION=fb1939ab92846761595833361c6b0b0ecd543861

export DEBIAN_FRONTEND=noninteractive

# Update apt-get lists
apt-get update -y

# Install dependencies
apt-get install -y --no-install-recommends \
    arcanist \
    bzr \
    ca-certificates \
    curl \
    ed \
    golang-go \
    gcc \
    libc6-dev \
    python-requests \
    python-requests-unixsocket \
    python3.5 \
    python3-minimal \
    python3-wheel \
    python3-pip \
    python3-venv \
    python3-requests \
    python3-requests-unixsocket \
    python3-setuptools \
    nodejs \
    npm \
    openssh-client \
    rsync \
    wget

mkdir -p /builds/worker/.mozbuild
chown -R worker:worker /builds/worker/
export GOPATH=/builds/worker/go

# pdf.js setup
# We want to aviod downloading a ton of packages all the time, so
# we will preload the pdf.js repo (and packages) in the Docker image
# and only update it at runtime. This means that the `./mach vendor`
# behavior for pdf.js will also be kind of custom
npm install -g gulp-cli
cd /builds/worker/
git clone https://github.com/mozilla/pdf.js.git
cd /builds/worker/pdf.js
npm ci

# Build Google's Cloud SQL Proxy from source
cd /builds/worker/
mkdir cloud_sql_proxy
cd cloud_sql_proxy
go mod init .
go get github.com/GoogleCloudPlatform/cloudsql-proxy/cmd/cloud_sql_proxy@$SQLPROXY_REVISION

# Check out source code
cd /builds/worker/
git clone https://github.com/mozilla-services/updatebot.git
cd updatebot
git checkout "$UPDATEBOT_REVISION"

# Set up dependencies
cd /builds/worker/
chown -R worker:worker .
chown -R worker:worker .*

python3 -m pip install -U pip
python3 -m pip install poetry

rm -rf /setup
