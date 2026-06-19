#!/usr/bin/env bash
set -e

git submodule sync
git submodule update --init --remote
git pull origin main
