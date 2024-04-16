#!/bin/bash
TEMP=$(getopt -n "$0" -a -l "transport:,url:,runtime:" -- -- "$@")

usage="usage: $(basename "$0") [-h] [--transport [{redispubsub_tcp,mq,redispubsub_socket}]] [--runtime [{docker,simer}]]\n\n
\n
where:\n
    --transport   Transport type \n
    --runtime    Container runtime"

if [ $# -eq 0 ]; then
    echo $usage
    exit 1
fi

eval set --  "$TEMP"

while [ $# -gt 0 ]
do
            case "$1" in
                --transport) TRANSPORT="$2"; shift;;
                --runtime) RUNTIME="$2"; shift;;
            esac
            shift;
done

export RUNTIME=$RUNTIME
export TRANSPORT=$TRANSPORT
echo "Runtime : $RUNTIME"
make