#!/bin/bash

# remove metadata chunks
# pip install wavchunk
# python3 -m wavchunk get --delete --data mydata.bin < infile.wav > outfile.wav

# ffmpeg -i input.mp3 -c:a libvorbis -q:a 4 output.ogg
# ffmpeg -acodec libvorbis -i "$file" -acodec pcm_s16le $wav
# # remove metadata
# ffmpeg -i input.wav -map_metadata -1 -acodec copy output.wav
# ffmpeg -i $1 -ar 22050 $1.wav

#@ffmpeg -i ding.wav -map_metadata -1 -acodec copy -write_id3v1 0 -metadata title="" -metadata artist="" -metadata album="" -metadata comment="" dingnochunk.wav

# seems to work
sox input.mp3 output.wav
