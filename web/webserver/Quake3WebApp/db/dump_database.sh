#!/bin/bash

#date=$(date +%F)
# TODO vacuum first
#pg_dump -U <you> -d quake3webapp -h localhost > ./quake3webapp_server_${date}.db

psql    -U <you> -d quake3webapp -h localhost -c 'vacuum;'
pg_dump -U <you> -d quake3webapp -h localhost > ./quake3webapp_server.db

