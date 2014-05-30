#!/bin/sh
# This script is only needed for Moloch deployments that monitor live traffic.
# It drops the old index and optimizes yesterdays index.
# It should be run once a day during non peak time.

# CONFIG
ESHOSTPORT=CHANGEMEHOST:CHANGEMEPORT
RETAINNUMDAYS=7

/data/moloch/db/db.pl $ESHOSTPORT expire daily $RETAINNUMDAYS
