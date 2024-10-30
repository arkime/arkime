#!/bin/bash
BASEDIR=/opt/arkime


if [ ! -f $BASEDIR/etc/config.ini ]; then
    echo "WARNING - Config file '$BASEDIR/etc/config.ini' not found"
fi

######################################################################
run_viewer_forever() {
    while true; do
        (cd $BASEDIR/viewer; $BASERDIR/bin/node viewer.js)
        sleep 1
    done
}

######################################################################
run_cont3xt_forever() {
    $BASEDIR/bin/arkime_config_interfaces.sh
    while true; do
        (cd $BASEDIR/cont3xt; $BASERDIR/bin/node cont3xt.js)
        sleep 1
    done
}

######################################################################
run_capture_forever() {
    $BASEDIR/bin/arkime_config_interfaces.sh
    while true; do
        (cd $BASEDIR/bin; ./capture)
        sleep 1
    done
}

######################################################################
# Function to kill all background processes on script exit
cleanup() {
    echo "Stopping all programs..."
    pkill -P $$  # Kill all child processes of the current script
    exit 0
}
# Trap SIGINT (Ctrl+C) and call the cleanup function
trap cleanup SIGINT


######################################################################
# Figure out what to run
case "$1" in
    viewer)
        echo "Running viewer"
        run_viewer_forever &
        ;;
    cont3xt)
        echo "Running cont3xt"
        run_cont3xt_forever &
        ;;
    capture)
        echo "capture"
        run_capture_forever &
        ;;
    capture-viewer)
        run_capture_forever &
        run_viewer_forever &
        ;;
    *)
        echo "Usage: $0 {viewer|capture|capture-viewer}"
        exit 1
        ;;
esac

wait

