# Moloch Query

## Dependencies

`moloch_query` requires the python packages elasticsearch and requests:

    > pip3 install requests elasticsearch

## Quick Start

`moloch_query` takes two-phase approach to get data. The first phase uses Elasticsearch metadata to narrow the number
of sessions searched. The second phase is optionally used to search or output the full session pcap.
The second phase is much slower, so creating a good metadata filter is important to limit the number of
sessions searched.

Results are written to stdout after every page, which is 1000 by default (See `-p` option below). All diagnostic
and progress information is written to stderr.

### Metadata Search

A metadata search uses the same query syntax as the Moloch UI and is supplied with the `-q` option. If no
output fields are supplied, it will return the total count of records matched.

    > /opt/moloch_tools/moloch_query --apiuser user --apipass pass --molurl https://localhost:8005 --esurl http://localhost:9200 -s "2017-10-11 00:00:00" -e "2017-10-11 11:59:59" -q "host.http == *.yopmail.com"
    626

If a set of output fields is supplied with the `-f` option it will output a tsv with the value of the fields to
stdout. If the `-h` option is given, then a header row is added. To see the fields allowed in the output,
run moloch\_query with the `-l` flag.

    > /opt/moloch_tools/moloch_query --apiuser user --apipass pass --molurl https://localhost:8005 --esurl http://localhost:9200 -s "2017-10-11 00:00:00" -e "2017-10-11 11:59:59" -f 'http.uri.path,host.http,tags' -q "host.http == *.yopmail.com" > out.tsv
    2017-10-16 15:17:37.747518 Getting fields from elasticsearch
    2017-10-16 15:17:37.908664 Done.
    2017-10-16 15:17:38.898286 100% : 626 sessions of 626, Elapsed: 0:00:01.150763, Remaining: 0:00:00

### Full Packet Search/Output

Adding the field `packet` to the output fields the hexified pcap of the session is added to the output.

For packet searches, either the `--regex` or `--hexregex` options are used. The `--regex` option works on the
raw packets and `--hexregex` works on the hexified packets.

## Options

* `-s DATE/TIME`  Start time for search
* `-e DATE/TIME`  End time for search
* `-q QUERY`  Query to filter the sessions. Uses the same syntax as the Moloch UI.
* `-f FIELDS`  A comma-separated list of fields to include in the output.
* `-l`  List the fields allowed for output (for the `-f` option).
* `-c NUMBER`  The number of concurrent session pcap download/searches. Only appliable for full packet search/output.
* `-p NUMBER`  Number of results to process at a time from elasticsearch. Also how often to report progress and flush results.
* `--headers`  Include a header row in the output.
* `--regex`  Regex to run on the raw pcap for each session.
* `--hexregex`  Regex to run on the hexified raw pcap for each session.
* `--limit NUMBER`  Limit the number of results returned to this.
* `--apiuser USER`  The username for Moloch.
* `--apipass PASS`  The password for Moloch.
* `--molurl URL`  The URL for moloch.
* `--esurl ES`  URL for elasticsearch
