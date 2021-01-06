#!/bin/bash

set -e

echo "Setting ulimit -l unlimited"
ulimit -l unlimited

exec "$@"
