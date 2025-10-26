#!/bin/bash
export BASEDIR=/opt/arkime

FOREVER=0

######################################################################
copy_elasticsearch() {
    if [[ -n "$ARKIME__elasticsearch" && -z "$ARKIME__usersElasticsearch" ]]; then
        export ARKIME__usersElasticsearch="$ARKIME__elasticsearch"
        if [[ -n "$ARKIME__elasticsearchBasicAuth" ]]; then
            export ARKIME__usersElasticsearchBasicAuth="$ARKIME__elasticsearchBasicAuth"
        fi
        if [[ -n "$ARKIME__prefix" ]]; then
            export ARKIME__usersPrefix="$ARKIME__prefix"
        fi
    fi

}
######################################################################
run_wise() {
    copy_elasticsearch
    while true; do
        (cd $BASEDIR/wiseService; $BASEDIR/bin/node wiseService.js "$@")
        if [ $FOREVER -eq 0 ]; then break; fi
        sleep 1

    done
}

######################################################################
run_parliament() {
    copy_elasticsearch
    while true; do
        (cd $BASEDIR/parliament; $BASEDIR/bin/node parliament.js "$@")
        if [ $FOREVER -eq 0 ]; then break; fi
        sleep 1
    done
}

######################################################################
run_viewer() {
    while true; do
        (cd $BASEDIR/viewer; $BASEDIR/bin/node viewer.js "$@")
        if [ $FOREVER -eq 0 ]; then break; fi
        sleep 1
    done
}

######################################################################
run_db() {
    $BASEDIR/db/db.pl "$@"
}

######################################################################
run_cont3xt() {
    copy_elasticsearch
    while true; do
        (cd $BASEDIR/cont3xt; $BASEDIR/bin/node cont3xt.js "$@")
        if [ $FOREVER -eq 0 ]; then break; fi
        sleep 1
    done
}

######################################################################
run_capture() {
    if [ ! -f $BASEDIR/etc/config.ini ]; then
        echo "WARNING - Config file '$BASEDIR/etc/config.ini' not found"
    fi

    $BASEDIR/bin/arkime_config_interfaces.sh
    while true; do
        (cd $BASEDIR/bin; ./capture "$@")
        if [ $FOREVER -eq 0 ]; then break; fi
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
show_help() {
    echo "Usage: $0 <command> [options] -- <command argument1> <command argument2> ..."
    echo "Commands:"
    echo "  capture            Run capture"
    echo "  capture-viewer     Run capture and viewer"
    echo "  db.pl              Run db.pl"
    echo "  viewer             Run viewer"
    echo "  cont3xt            Run cont3xt"
    echo "  parliament         Run parliament"
    echo "  wise               Run wise"
    echo
    echo "Options:"
    echo "  --add-admin        Add a admin user if missing, please change password ASAP"
    echo "  --basedir <dir>    Use a different base directory for Arkime, default is /opt/arkime"
    echo "  --forever          Run the tools forever, default is just once"
    echo "  --init <dburl>     Run db.pl init if needed, not recommended"
    echo "  --update-geo       Run /opt/arkime/bin/arkime_update_geo.sh"
    echo "  --upgrade <dburl>  Run db.pl upgrade if needed, not recommended"
    echo "  --                 All arguments after this are passed to the command"
    echo
}

######################################################################
# Check if no arguments were provided
if [ $# -eq 0 ]; then
    show_help
    exit 1
fi

# Save command
command="$1"
shift

# Parse options
while [ $# -gt 0 ]; do
    case "$1" in
        --add-admin)
            echo "Trying to add admin/admin user if missing, please change password ASAP"
            (cd $BASEDIR/viewer; $BASEDIR/bin/node addUser.js --insecure admin admin admin --admin --createOnly)
            shift
            ;;
        --basedir)
            shift
            BASEDIR=$1
            shift
            ;;
        --forever)
            FOREVER=1
            shift
            ;;
        --init)
            shift
            DBURL=$1
            shift
            $BASEDIR/db/db.pl --insecure "$DBURL" init --ifneeded
            ;;
        --upgrade)
            shift
            DBURL=$1
            shift
            $BASEDIR/db/db.pl --insecure "$DBURL" upgradenoprompt --ifneeded
            ;;
        --update-geo)
            echo "Updating GeoIP databases"
            $BASEDIR/bin/arkime_update_geo.sh
            shift
            ;;
        --)
            shift
            break
            ;;
        *)
            break
            ;;
    esac
done

# Figure out what to run
case "$command" in
    wise)
        echo "Starting wise"
        run_wise "$@"
        ;;
    viewer)
        echo "Starting viewer"
        run_viewer "$@"
        ;;
    parliament)
        echo "Starting parliament"
        run_parliament "$@"
        ;;
    db|db.pl)
        echo "Starting db"
        run_db "$@"
        ;;
    cont3xt)
        echo "Starting cont3xt"
        run_cont3xt "$@"
        ;;
    capture)
        echo "Starting capture"
        run_capture "$@"
        ;;
    capture-viewer)
        echo "Starting capture"
        run_capture "$@" &
        echo "Starting viewer"
        run_viewer "$@" &
        wait
        ;;
    *)
        show_help
        exit 1
        ;;
esac

