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
run_forever() {
    local dir="$1"
    shift
    while true; do
        (cd "$dir" && "$@")
        if [ $FOREVER -eq 0 ]; then break; fi
        sleep 1
    done
}

######################################################################
run_wise() {
    copy_elasticsearch
    run_forever "$BASEDIR/wiseService" "$BASEDIR/bin/node" wiseService.js "$@"
}

######################################################################
run_parliament() {
    copy_elasticsearch
    run_forever "$BASEDIR/parliament" "$BASEDIR/bin/node" parliament.js "$@"
}

######################################################################
run_viewer() {
    run_forever "$BASEDIR/viewer" "$BASEDIR/bin/node" viewer.js "$@"
}

######################################################################
run_db() {
    $BASEDIR/db/db.pl "$@"
}

######################################################################
run_cont3xt() {
    copy_elasticsearch
    run_forever "$BASEDIR/cont3xt" "$BASEDIR/bin/node" cont3xt.js "$@"
}

######################################################################
run_capture() {
    if [ ! -f $BASEDIR/etc/config.ini ]; then
        echo "WARNING - Config file '$BASEDIR/etc/config.ini' not found"
    fi

    $BASEDIR/bin/arkime_config_interfaces.sh
    run_forever "$BASEDIR/bin" ./capture "$@"
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
    echo "  capture              Run capture"
    echo "  capture-viewer       Run capture and viewer"
    echo "  db.pl                Run db.pl"
    echo "  viewer               Run viewer"
    echo "  cont3xt              Run cont3xt"
    echo "  parliament           Run parliament"
    echo "  wise                 Run wise"
    echo
    echo "Options:"
    echo "  --add-admin            Add an admin user if missing, please change password ASAP"
    echo "  --basedir <dir>        Use a different base directory for Arkime, default is /opt/arkime"
    echo "  --db <args>            Run db.pl with args, can be specified multiple times"
    echo "  --forever              Run the tools forever, default is just once"
    echo "  --update-geo           Run /opt/arkime/bin/arkime_update_geo.sh"
    echo "  --wait-for-db <dburl>  Wait for Elasticsearch/OpenSearch to be ready before running command"
    echo "  --                     All arguments after this are passed to the command"
    echo
    echo "Examples:"
    echo "  $0 capture --db 'http://es:9200 init --ifneeded'"
    echo "  $0 capture --db 'http://es:9200 init --ifneeded --ilm' --db 'http://es:9200 ilm 12h 7d'"
    echo "  $0 capture --db 'http://es:9200 upgradenoprompt --ifneeded --compression gzip'"
    echo
    echo "Deprecated (use --db instead):"
    echo "  --init <dburl>         Use: --db '<dburl> init --ifneeded'"
    echo "  --upgrade <dburl>      Use: --db '<dburl> upgradenoprompt --ifneeded'"
    echo "  --ilm <force> <delete> Use: --db '<dburl> init --ifneeded --ilm' --db '<dburl> ilm <force> <delete>'"
    echo "  --ism <force> <delete> Use: --db '<dburl> init --ifneeded --ism' --db '<dburl> ism <force> <delete>'"
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
DOINIT=0
DOUPGRADE=0
ISM_OPTION=""
ISM_PARAM=""
ILM_OPTION=""
ILM_PARAM=""
DB_COMMANDS=()
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
        --db)
            shift
            DB_COMMANDS+=("$1")
            shift
            ;;
        --forever)
            FOREVER=1
            shift
            ;;
        --ilm)
            if [ -n "$ISM_OPTION" ]; then
                echo "Error: Cannot specify both --ism and --ilm"
                exit 1
            fi
            shift
            ILM_OPTION=$1
            shift
            ILM_PARAM=$1
            shift
            ;;
        --init)
            shift
            DBURL=$1
            shift
            DOINIT=1
            ;;
        --ism)
            if [ -n "$ILM_OPTION" ]; then
                echo "Error: Cannot specify both --ism and --ilm"
                exit 1
            fi
            shift
            ISM_OPTION=$1
            shift
            ISM_PARAM=$1
            shift
            ;;
        --upgrade)
            shift
            DBURL=$1
            shift
            DOUPGRADE=1
            ;;
        --update-geo)
            echo "Updating GeoIP databases"
            $BASEDIR/bin/arkime_update_geo.sh
            shift
            ;;
        --wait-for-db)
            shift
            WAIT_DB_URL=$1
            echo "Waiting for Elasticsearch/OpenSearch to be ready..."
            CURL_OPTS="-sf"
            if [ -n "$ARKIME__elasticsearchBasicAuth" ]; then
                CURL_OPTS="$CURL_OPTS --user $ARKIME__elasticsearchBasicAuth"
            fi
            if [ -n "$ARKIME__insecure" ]; then
                CURL_OPTS="$CURL_OPTS -k"
            fi
            until curl $CURL_OPTS "$WAIT_DB_URL/_cluster/health?wait_for_status=yellow&timeout=30s"; do
                echo "Waiting for Elasticsearch/OpenSearch..."
                sleep 2
            done
            echo "Elasticsearch/OpenSearch is ready"
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

# Handle legacy --init/--upgrade with --ilm/--ism flags
if [ $DOINIT -eq 1 ]; then
    if [ -n "$ISM_OPTION" ]; then
        DB_COMMANDS+=("$DBURL init --ifneeded --ism")
        DB_COMMANDS+=("$DBURL ism $ISM_OPTION $ISM_PARAM")
    elif [ -n "$ILM_OPTION" ]; then
        DB_COMMANDS+=("$DBURL init --ifneeded --ilm")
        DB_COMMANDS+=("$DBURL ilm $ILM_OPTION $ILM_PARAM")
    else
        DB_COMMANDS+=("$DBURL init --ifneeded")
    fi
elif [ $DOUPGRADE -eq 1 ]; then
    if [ -n "$ISM_OPTION" ]; then
        DB_COMMANDS+=("$DBURL upgradenoprompt --ifneeded --ism")
        DB_COMMANDS+=("$DBURL ism $ISM_OPTION $ISM_PARAM")
    elif [ -n "$ILM_OPTION" ]; then
        DB_COMMANDS+=("$DBURL upgradenoprompt --ifneeded --ilm")
        DB_COMMANDS+=("$DBURL ilm $ILM_OPTION $ILM_PARAM")
    else
        DB_COMMANDS+=("$DBURL upgradenoprompt --ifneeded")
    fi
fi

# Run all db.pl commands
for db_cmd in "${DB_COMMANDS[@]}"; do
    $BASEDIR/db/db.pl --insecure $db_cmd || exit 1
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

