#!/bin/bash

gst-launch-1.0 playbin uri=file://"$1" audio-sink='pulsesink stream-properties="props,media.role=navigator,zone.name=driver"'
