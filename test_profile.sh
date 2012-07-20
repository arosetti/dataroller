#!/bin/sh

./dataroller $*
gprof dataroller --brief
