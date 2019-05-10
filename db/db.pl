#!/usr/bin/perl
# This script can initialize, upgrade or provide simple maintenance for the
# moloch elastic search db
#
# Schema Versions
#  0 - Before this script existed
#  1 - First version of script; turned on strict schema; added lpms, fpms; added
#      many missing items that were dynamically created by mistake
#  2 - Added email items
#  3 - Added email md5
#  4 - Added email host and ip; added help, usersimport, usersexport, wipe commands
#  5 - No schema change, new rotate command, encoding of file pos is different.
#      Negative is file num, positive is file pos
#  6 - Multi fields for spi view, added xffcnt, 0.90 fixes, need to type INIT/UPGRADE
#      instead of YES
#  7 - files_v3
#  8 - fileSize, memory to stats/dstats and -v flag
#  9 - http body hash, rawus
# 10 - dynamic fields for http and email headers
# 11 - Require 0.90.1, switch from soft to node, new fpd field, removed fpms field
# 12 - Added hsver, hdver fields, diskQueue, user settings, scrub* fields, user removeEnabled
# 13 - Rename rotate to expire, added smb, socks, rir fields
# 14 - New http fields, user.views
# 15 - New first byte fields, socks user
# 16 - New dynamic plugin section
# 17 - email hasheader, db,pa,by src and dst
# 18 - fields db
# 19 - users_v3
# 20 - queries
# 21 - doc_values, new tls fields, starttime/stoptime/view
# 22 - cpu to stats/dstats
# 23 - packet lengths
# 24 - field category
# 25 - cert hash
# 26 - dynamic stats, ES 2.0
# 27 - table states
# 28 - timestamp, firstPacket, lastPacket, ipSrc, ipDst, portSrc, portSrc
# 29 - stats/dstats uses dynamic_templates
# 30 - change file to dynamic
# 31 - Require ES >= 2.4, dstats_v2, stats_v1
# 32 - Require ES >= 2.4 or ES >= 5.1.2, tags_v3, queries_v1, fields_v1, users_v4, files_v4, sequence_v1
# 33 - user columnConfigs
# 34 - stats_v2
# 35 - user spiviewFieldConfigs
# 36 - user action history
# 37 - add request body to history
# 50 - Moloch 1.0
# 51 - Upgrade for ES 6.x: sequence_v2, fields_v2, queries_v2, files_v5, users_v5, dstats_v3, stats_v3
# 52 - Hunt (packet search)
# 53 - add forcedExpression to history
# 54 - users_v6
# 55 - user hideStats, hideFiles, hidePcap, and disablePcapDownload
# 56 - notifiers
# 57 - hunt notifiers
# 58 - users message count and last used date
# 59 - tokens
# 60 - users time query limit

use HTTP::Request::Common;
use LWP::UserAgent;
use JSON;
use Data::Dumper;
use POSIX;
use strict;

my $VERSION = 60;
my $verbose = 0;
my $PREFIX = "";
my $NOCHANGES = 0;
my $SHARDS = -1;
my $REPLICAS = -1;
my $HISTORY = 13;
my $SEGMENTS = 1;
my $NOOPTIMIZE = 0;
my $FULL = 0;
my $REVERSE = 0;
my $SHARDSPERNODE = 1;
my $ESTIMEOUT=60;
my $UPGRADEALLSESSIONS = 1;
my $DOHOTWARM = 0;
my $WARMAFTER = -1;

#use LWP::ConsoleLogger::Everywhere ();

################################################################################
sub MIN ($$) { $_[$_[0] > $_[1]] }
sub MAX ($$) { $_[$_[0] < $_[1]] }

sub commify {
    scalar reverse join ',',
    unpack '(A3)*',
    scalar reverse shift
}

################################################################################
sub logmsg
{
  local $| = 1;
  print (scalar localtime() . " ") if ($verbose > 0);
  print ("@_");
}
################################################################################
sub showHelp($)
{
    my ($str) = @_;
    print "\n", $str,"\n\n";
    print "$0 [Global Options] <ESHOST:ESPORT> <command> [<command arguments>]\n";
    print "\n";
    print "Global Options:\n";
    print "  -v                           - Verbose, multiple increases level\n";
    print "  --prefix <prefix>            - Prefix for table names\n";
    print "  -n                           - Make no db changes\n";
    print "  --timeout <timeout>          - Timeout in seconds for ES, default 60\n";
    print "\n";
    print "General Commands:\n";
    print "  info                         - Information about the database\n";
    print "  init [<opts>]                - Clear ALL elasticsearch moloch data and create schema\n";
    print "    --shards <shards>          - Number of shards for sessions, default number of nodes\n";
    print "    --replicas <num>           - Number of replicas for sessions, default 0\n";
    print "    --shardsPerNode <shards>   - Number of shards per node or use \"null\" to let ES decide, default shards*replicas/nodes\n";
    print "    --hotwarm                  - Set 'hot' for 'node.attr.molochtype' on new indices\n";
    print "  wipe                         - Same as init, but leaves user database untouched\n";
    print "  upgrade [<opts>]             - Upgrade Moloch's schema in elasticsearch from previous versions\n";
    print "    --shards <shards>          - Number of shards for sessions, default number of nodes\n";
    print "    --replicas <num>           - Number of replicas for sessions, default 0\n";
    print "    --shardsPerNode <shards>   - Number of shards per node or use \"null\" to let ES decide, default shards*replicas/nodes\n";
    print "    --hotwarm                  - Set 'hot' for 'node.attr.molochtype' on new indices\n";
    print "  expire <type> <num> [<opts>] - Perform daily ES maintenance and optimize all indices in ES\n";
    print "       type                    - Same as rotateIndex in ini file = hourly,hourlyN,daily,weekly,monthly\n";
    print "       num                     - number of indexes to keep\n";
    print "    --replicas <num>           - Number of replicas for older sessions indices, default 0\n";
    print "    --nooptimize               - Do not optimize session indexes during this operation\n";
    print "    --history <num>            - Number of weeks of history to keep, default 13\n";
    print "    --segments <num>           - Number of segments to optimize sessions to, default 1\n";
    print "    --reverse                  - Optimize from most recent to oldest\n";
    print "    --shardsPerNode <shards>   - Number of shards per node or use \"null\" to let ES decide, default shards*replicas/nodes\n";
    print "    --warmafter <num>          - Set molochwarm on indices after <num> <tpye>\n";
    print "  optimize                     - Optimize all indices in ES\n";
    print "    --segments <num>           - Number of segments to optimize sessions to, default 1\n";
    print "  disableusers <days>          - Disable user accounts that have not been active\n";
    print "      days                     - Number of days of inactivity (integer)\n";
    print "\n";
    print "Backup and Restore Commands:\n";
    print "  backup <basename>            - Backup everything but sessions; filenames created start with <basename>\n";
    print "  restore <basename> [<opts>]  - Restore everything but sessions; filenames restored from start with <basename>\n";
    print "    --skipupgradeall           - Do not upgrade Sessions\n";
    print "  export <index> <basename>    - Save a single index into a file, filename starts with <basename>\n";
    print "  import <filename>            - Import single index from <filename>\n";
    print "  users-export <filename>      - Save the users info to <filename>\n";
    print "  users-import <filename>      - Load the users info from <filename>\n";
    print "\n";
    print "File Commands:\n";
    print "  mv <old fn> <new fn>         - Move a pcap file in the database (doesn't change disk)\n";
    print "  rm <fn>                      - Remove a pcap file in the database (doesn't change disk)\n";
    print "  rm-missing <node>            - Remove from db any MISSING files on THIS machine for the named node\n";
    print "  add-missing <node> <dir>     - Add to db any MISSING files on THIS machine for named node and directory\n";
    print "  sync-files  <nodes> <dirs>   - Add/Remove in db any MISSING files on THIS machine for named node(s) and directory(s), both comma separated\n";
    print "\n";
    print "Field Commands:\n";
    print "  field disable <exp>          - disable a field from being indexed\n";
    print "  field enable <exp>           - enable a field from being indexed\n";
    print "\n";
    print "Node Commands:\n";
    print "  rm-node <node>               - Remove from db all data for node (doesn't change disk)\n";
    print "  add-alias <node> <hostname>  - Adds a hidden node that points to hostname\n";
    print "  hide-node <node>             - Hide node in stats display\n";
    print "  unhide-node <node>           - Unhide node in stats display\n";
    print "\n";
    print "ES maintenance\n";
    print "  set-replicas <pat> <num>              - Set the number of replicas for index pattern\n";
    print "  set-shards-per-node <pat> <num>       - Set the number of replicas for index pattern\n";
    print "  set-allocation-enable <mode>          - Set the allocation mode (all, primaries, new_primaries, none, null)\n";
    print "  allocate-empty <node> <index> <shard> - Allocate a empty shard on a node, DATA LOSS!\n";
    print "  unflood-stage <pat>                   - Mark index pattern as no longer flooded\n";
    exit 1;
}
################################################################################
sub waitFor
{
    my ($str, $help) = @_;

    print "Type \"$str\" to continue - $help?\n";
    while (1) {
        my $answer = <STDIN>;
        chomp $answer;
        last if ($answer eq $str);
        print "You didn't type \"$str\", for some reason you typed \"$answer\"\n";
    }
}

################################################################################
sub esIndexExists
{
    my ($index) = @_;
    logmsg "HEAD ${main::elasticsearch}/$index\n" if ($verbose > 2);
    my $response = $main::userAgent->head("${main::elasticsearch}/$index");
    logmsg "HEAD RESULT:", $response->code, "\n" if ($verbose > 3);
    return $response->code == 200;
}
################################################################################
sub esCheckAlias
{
    my ($alias, $index) = @_;
    my $result = esGet("/_alias/$alias", 1);

    return (exists $result->{$index} && exists $result->{$index}->{aliases}->{$alias});
}
################################################################################
sub esGet
{
    my ($url, $dontcheck) = @_;
    logmsg "GET ${main::elasticsearch}$url\n" if ($verbose > 2);
    my $response = $main::userAgent->get("${main::elasticsearch}$url");
    if (($response->code == 500 && $ARGV[1] ne "init") || ($response->code != 200 && !$dontcheck)) {
      die "Couldn't GET ${main::elasticsearch}$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?";
    }
    my $json = from_json($response->content);
    logmsg "GET RESULT:", Dumper($json), "\n" if ($verbose > 3);
    return $json
}

################################################################################
sub esPost
{
    my ($url, $content, $dontcheck) = @_;

    if ($NOCHANGES && $url !~ /_search/) {
      logmsg "NOCHANGE: POST ${main::elasticsearch}$url\n";
      return;
    }

    logmsg "POST ${main::elasticsearch}$url\n" if ($verbose > 2);
    logmsg "POST DATA:", Dumper($content), "\n" if ($verbose > 3);
    my $response = $main::userAgent->post("${main::elasticsearch}$url", Content => $content, Content_Type => "application/json");
    if ($response->code == 500 || ($response->code != 200 && $response->code != 201 && !$dontcheck)) {
      return from_json("{}") if ($dontcheck == 2);

      logmsg "POST RESULT:", $response->content, "\n" if ($verbose > 3);
      die "Couldn't POST ${main::elasticsearch}$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?";
    }

    my $json = from_json($response->content);
    logmsg "POST RESULT:", Dumper($json), "\n" if ($verbose > 3);
    return $json
}

################################################################################
sub esPut
{
    my ($url, $content, $dontcheck) = @_;

    if ($NOCHANGES) {
      logmsg "NOCHANGE: PUT ${main::elasticsearch}$url\n";
      return;
    }

    logmsg "PUT ${main::elasticsearch}$url\n" if ($verbose > 2);
    logmsg "PUT DATA:", Dumper($content), "\n" if ($verbose > 3);
    my $response = $main::userAgent->request(HTTP::Request::Common::PUT("${main::elasticsearch}$url", Content => $content, Content_Type => "application/json"));
    if ($response->code == 500 || ($response->code != 200 && !$dontcheck)) {
      logmsg Dumper($response);
      die "Couldn't PUT ${main::elasticsearch}$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?\n" . $response->content;
    }

    my $json = from_json($response->content);
    logmsg "PUT RESULT:", Dumper($json), "\n" if ($verbose > 3);
    return $json
}

################################################################################
sub esDelete
{
    my ($url, $dontcheck) = @_;

    if ($NOCHANGES) {
      logmsg "NOCHANGE: DELETE ${main::elasticsearch}$url\n";
      return;
    }

    logmsg "DELETE ${main::elasticsearch}$url\n" if ($verbose > 2);
    my $response = $main::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "${main::elasticsearch}$url"));
    if ($response->code == 500 || ($response->code != 200 && !$dontcheck)) {
      die "Couldn't DELETE ${main::elasticsearch}$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?";
    }
    my $json = from_json($response->content);
    return $json
}

################################################################################
sub esCopy
{
    my ($srci, $dsti) = @_;

    $main::userAgent->timeout(7200);

    my $status = esGet("/_stats/docs", 1);
    logmsg "Copying " . $status->{indices}->{$PREFIX . $srci}->{primaries}->{docs}->{count} . " elements from ${PREFIX}$srci to ${PREFIX}$dsti\n";

    esPost("/_reindex", to_json({"source" => {"index" => $PREFIX.$srci}, "dest" => {"index" => $PREFIX.$dsti, "version_type" => "external"}, "conflicts" => "proceed"}));

    my $status = esGet("/${PREFIX}${dsti}/_refresh", 1);
    my $status = esGet("/_stats/docs", 1);
    if ($status->{indices}->{$PREFIX . $srci}->{primaries}->{docs}->{count} > $status->{indices}->{$PREFIX . $dsti}->{primaries}->{docs}->{count}) {
        logmsg $status->{indices}->{$PREFIX . $srci}->{primaries}->{docs}->{count}, " > ",  $status->{indices}->{$PREFIX . $dsti}->{primaries}->{docs}->{count}, "\n";
        die "\nERROR - Copy failed from $srci to $dsti, you will probably need to delete $dsti and run upgrade again.  Make sure to not change the index while upgrading.\n\n";
    }

    logmsg "\n";
    $main::userAgent->timeout($ESTIMEOUT + 5);
}
################################################################################
sub esScroll
{
    my ($index, $type, $query) = @_;

    my @hits = ();

    my $id = "";
    while (1) {
        if ($verbose > 0) {
            local $| = 1;
            print ".";
        }
        my $url;
        if ($id eq "") {
            if ($type eq "") {
                $url = "/${PREFIX}$index/_search?scroll=10m&size=500";
            } else {
                $url = "/${PREFIX}$index/$type/_search?scroll=10m&size=500";
            }
        } else {
            $url = "/_search/scroll?scroll=10m&scroll_id=$id";
            $query = "";
        }


        my $incoming = esPost($url, $query, 1);
        die Dumper($incoming) if ($incoming->{status} == 404);
        last if (@{$incoming->{hits}->{hits}} == 0);

        push(@hits, @{$incoming->{hits}->{hits}});

        $id = $incoming->{_scroll_id};
    }
    return \@hits;
}
################################################################################
sub esAlias
{
    my ($cmd, $index, $alias, $dontaddprefix) = @_;
    logmsg "Alias cmd $cmd from $index to alias $alias\n" if ($verbose > 0);
    if (!$dontaddprefix){ # append PREFIX
    esPost("/_aliases", '{ "actions": [ { "' . $cmd . '": { "index": "' . $PREFIX . $index . '", "alias" : "'. $PREFIX . $alias .'" } } ] }', 1);
    } else { # do not append PREFIX
        esPost("/_aliases", '{ "actions": [ { "' . $cmd . '": { "index": "' . $index . '", "alias" : "'. $alias .'" } } ] }', 1);
    }
}

################################################################################
sub esWaitForNoTask
{
    my ($str) = @_;
    while (1) {
        logmsg "GET ${main::elasticsearch}/_cat/tasks\n" if ($verbose > 1);
        my $response = $main::userAgent->get("${main::elasticsearch}/_cat/tasks");
        if ($response->code != 200) {
            sleep(30);
        }

        return 1 if (index ($response->content, $str) == -1);
        sleep 20;
    }
}
################################################################################
sub esForceMerge
{
    my ($index, $segments) = @_;
    esWaitForNoTask("forcemerge");
    esPost("/$index/_forcemerge?max_num_segments=$segments", "", 2);
    esWaitForNoTask("forcemerge");
}

################################################################################
sub sequenceCreate
{
    my $settings = '
{
  "settings": {
    "index.priority": 100,
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    logmsg "Creating sequence_v2 index\n" if ($verbose > 0);
    esPut("/${PREFIX}sequence_v2", $settings, 1);
    esAlias("add", "sequence_v2", "sequence");
    sequenceUpdate();
}

################################################################################
sub sequenceUpdate
{
    my $mapping = '
{
  "sequence": {
    "_source" : { "enabled": "false" },
    "enabled" : "false"
  }
}';

    logmsg "Setting sequence_v2 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}sequence_v2/sequence/_mapping?master_timeout=${ESTIMEOUT}s", $mapping);
}
################################################################################
sub sequenceUpgrade
{

    if (esCheckAlias("${PREFIX}sequence", "${PREFIX}sequence_v2") && esIndexExists("${PREFIX}sequence_v2")) {
        logmsg ("SKIPPING - ${PREFIX}sequence already points to ${PREFIX}sequence_v2\n");
        return;
    }

    $main::userAgent->timeout(7200);
    sequenceCreate();
    esAlias("remove", "sequence_v1", "sequence");
    my $results = esGet("/${PREFIX}sequence_v1/_search?version=true&size=10000", 0);

    logmsg "Copying " . $results->{hits}->{total} . " elements from ${PREFIX}sequence_v1 to ${PREFIX}sequence_v2\n";

    return if ($results->{hits}->{total} == 0);

    foreach my $hit (@{$results->{hits}->{hits}}) {
        esPost("/${PREFIX}sequence_v2/sequence/$hit->{_id}?version_type=external&version=$hit->{_version}", "{}", 1);
    }
    esDelete("/${PREFIX}sequence_v1");
    $main::userAgent->timeout($ESTIMEOUT + 5);
}
################################################################################
sub filesCreate
{
    my $settings = '
{
  "settings": {
    "index.priority": 80,
    "number_of_shards": 2,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    logmsg "Creating files_v5 index\n" if ($verbose > 0);
    esPut("/${PREFIX}files_v5", $settings);
    esAlias("add", "files_v5", "files");
    filesUpdate();
}
################################################################################
sub filesUpdate
{
    my $mapping = '
{
  "file": {
    "_source": {"enabled": "true"},
    "dynamic": "true",
    "dynamic_templates": [
      {
        "any": {
          "match": "*",
          "mapping": {
            "index": false
          }
        }
      }
    ],
    "properties": {
      "num": {
        "type": "long"
      },
      "node": {
        "type": "keyword"
      },
      "first": {
        "type": "long"
      },
      "name": {
        "type": "keyword"
      },
      "filesize": {
        "type": "long"
      },
      "locked": {
        "type": "short"
      },
      "last": {
        "type": "long"
      }
    }
  }
}';

    logmsg "Setting files_v5 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}files_v5/file/_mapping?master_timeout=${ESTIMEOUT}s", $mapping);
}
################################################################################
sub statsCreate
{
    my $settings = '
{
  "settings": {
    "index.priority": 70,
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    logmsg "Creating stats index\n" if ($verbose > 0);
    esPut("/${PREFIX}stats_v3", $settings);
    esAlias("add", "stats_v3", "stats");
    statsUpdate();
}

################################################################################
sub statsUpdate
{
my $mapping = '
{
  "stat": {
    "_source": {"enabled": "true"},
    "dynamic": "true",
    "dynamic_templates": [
      {
        "numeric": {
          "match_mapping_type": "long",
          "mapping": {
            "type": "long"
          }
        }
      }
    ],
    "properties": {
      "hostname": {
        "type": "keyword"
      },
      "nodeName": {
        "type": "keyword"
      },
      "currentTime": {
        "type": "date",
        "format": "epoch_second"
      }
    }
  }
}';

    logmsg "Setting stats mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}stats_v3/stat/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping, 1);
}
################################################################################
sub dstatsCreate
{
    my $settings = '
{
  "settings": {
    "index.priority": 50,
    "number_of_shards": 2,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    logmsg "Creating dstats_v3 index\n" if ($verbose > 0);
    esPut("/${PREFIX}dstats_v3", $settings);
    esAlias("add", "dstats_v3", "dstats");
    dstatsUpdate();
}

################################################################################
sub dstatsUpdate
{
my $mapping = '
{
  "dstat": {
    "_source": {"enabled": "true"},
    "dynamic": "true",
    "dynamic_templates": [
      {
        "numeric": {
          "match_mapping_type": "long",
          "mapping": {
            "type": "long",
            "index": false
          }
        }
      },
      {
        "noindex": {
          "match": "*",
          "mapping": {
            "index": false
          }
        }
      }
    ],
    "properties": {
      "nodeName": {
        "type": "keyword"
      },
      "interval": {
        "type": "short"
      },
      "currentTime": {
        "type": "date",
        "format": "epoch_second"
      }
    }
  }
}';

    logmsg "Setting dstats_v3 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}dstats_v3/dstat/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping, 1);
}
################################################################################
sub fieldsCreate
{
    my $settings = '
{
  "settings": {
    "index.priority": 90,
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    logmsg "Creating fields index\n" if ($verbose > 0);
    esPut("/${PREFIX}fields_v2", $settings);
    esAlias("add", "fields_v2", "fields");
    fieldsUpdate();
}
################################################################################
# Not the fix I want, but it works for now
sub fieldsIpDst
{
    esPost("/${PREFIX}fields_v2/field/ip.dst", '{
      "friendlyName": "Dst IP",
      "group": "general",
      "help": "Destination IP",
      "type": "ip",
      "dbField": "a2",
      "dbField2": "dstIp",
      "portField": "p2",
      "portField2": "dstPort",
      "category": "ip",
      "aliases": ["ip.dst:port"]
    }');
}
################################################################################
sub fieldsUpdate
{
    my $mapping = '
{
  "field": {
    "_source": {"enabled": "true"},
    "dynamic_templates": [
      {
        "string_template": {
          "match_mapping_type": "string",
          "mapping": {
            "type": "keyword"
          }
        }
      }
    ]
  }
}';

    logmsg "Setting fields_v2 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}fields_v2/field/_mapping?master_timeout=${ESTIMEOUT}s", $mapping);

    esPost("/${PREFIX}fields_v2/field/ip", '{
      "friendlyName": "All IP fields",
      "group": "general",
      "help": "Search all ip fields",
      "type": "ip",
      "dbField": "ipall",
      "dbField2": "ipall",
      "portField": "portall",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v2/field/port", '{
      "friendlyName": "All port fields",
      "group": "general",
      "help": "Search all port fields",
      "type": "integer",
      "dbField": "portall",
      "dbField2": "portall",
      "regex": "(^port\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.port$)"
    }');
    esPost("/${PREFIX}fields_v2/field/rir", '{
      "friendlyName": "All rir fields",
      "group": "general",
      "help": "Search all rir fields",
      "type": "uptermfield",
      "dbField": "rirall",
      "dbField2": "rirall",
      "regex": "(^rir\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.rir$)"
    }');
    esPost("/${PREFIX}fields_v2/field/country", '{
      "friendlyName": "All country fields",
      "group": "general",
      "help": "Search all country fields",
      "type": "uptermfield",
      "dbField": "geoall",
      "dbField2": "geoall",
      "regex": "(^country\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.country$)"
    }');
    esPost("/${PREFIX}fields_v2/field/asn", '{
      "friendlyName": "All ASN fields",
      "group": "general",
      "help": "Search all ASN fields",
      "type": "termfield",
      "dbField": "asnall",
      "dbField2": "asnall",
      "regex": "(^asn\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.asn$)"
    }');
    esPost("/${PREFIX}fields_v2/field/host", '{
      "friendlyName": "All Host fields",
      "group": "general",
      "help": "Search all Host fields",
      "type": "lotermfield",
      "dbField": "hostall",
      "dbField2": "hostall",
      "regex": "(^host\\\\.(?:(?!\\\\.(cnt|tokens)$).)*$|\\\\.host$)"
    }');
    esPost("/${PREFIX}fields_v2/field/ip.src", '{
      "friendlyName": "Src IP",
      "group": "general",
      "help": "Source IP",
      "type": "ip",
      "dbField": "a1",
      "dbField2": "srcIp",
      "portField": "p1",
      "portField2": "srcPort",
      "category": "ip"
    }');
    esPost("/${PREFIX}fields_v2/field/port.src", '{
      "friendlyName": "Src Port",
      "group": "general",
      "help": "Source Port",
      "type": "integer",
      "dbField": "p1",
      "dbField2": "srcPort",
      "category": "port"
    }');
    esPost("/${PREFIX}fields_v2/field/asn.src", '{
      "friendlyName": "Src ASN",
      "group": "general",
      "help": "GeoIP ASN string calculated from the source IP",
      "type": "termfield",
      "dbField": "as1",
      "dbField2": "srcASN",
      "rawField": "rawas1",
      "category": "asn"
    }');
    esPost("/${PREFIX}fields_v2/field/country.src", '{
      "friendlyName": "Src Country",
      "group": "general",
      "help": "Source Country",
      "type": "uptermfield",
      "dbField": "g1",
      "dbField2": "srcGEO",
      "category": "country"
    }');
    esPost("/${PREFIX}fields_v2/field/rir.src", '{
      "friendlyName": "Src RIR",
      "group": "general",
      "help": "Source RIR",
      "type": "uptermfield",
      "dbField": "rir1",
      "dbField2": "srcRIR",
      "category": "rir"
    }');
    fieldsIpDst();
    esPost("/${PREFIX}fields_v2/field/port.dst", '{
      "friendlyName": "Dst Port",
      "group": "general",
      "help": "Source Port",
      "type": "integer",
      "dbField": "p2",
      "dbField2": "dstPort",
      "category": "port"
    }');
    esPost("/${PREFIX}fields_v2/field/asn.dst", '{
      "friendlyName": "Dst ASN",
      "group": "general",
      "help": "GeoIP ASN string calculated from the destination IP",
      "type": "termfield",
      "dbField": "as2",
      "dbField2": "dstASN",
      "rawField": "rawas2",
      "category": "asn"
    }');
    esPost("/${PREFIX}fields_v2/field/country.dst", '{
      "friendlyName": "Dst Country",
      "group": "general",
      "help": "Destination Country",
      "type": "uptermfield",
      "dbField": "g2",
      "dbField2": "dstGEO",
      "category": "country"
    }');
    esPost("/${PREFIX}fields_v2/field/rir.dst", '{
      "friendlyName": "Dst RIR",
      "group": "general",
      "help": "Destination RIR",
      "type": "uptermfield",
      "dbField": "rir2",
      "dbField2": "dstRIR",
      "category": "rir"
    }');
    esPost("/${PREFIX}fields_v2/field/bytes", '{
      "friendlyName": "Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent AND received in a session",
      "type": "integer",
      "dbField": "by",
      "dbField2": "totBytes"
    }');
    esPost("/${PREFIX}fields_v2/field/bytes.src", '{
      "friendlyName": "Src Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent by source in a session",
      "type": "integer",
      "dbField": "by1",
      "dbField2": "srcBytes"
    }');
    esPost("/${PREFIX}fields_v2/field/bytes.dst", '{
      "friendlyName": "Dst Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent by destination in a session",
      "type": "integer",
      "dbField": "by2",
      "dbField2": "dstBytes"
    }');
    esPost("/${PREFIX}fields_v2/field/databytes", '{
      "friendlyName": "Data bytes",
      "group": "general",
      "help": "Total number of data bytes sent AND received in a session",
      "type": "integer",
      "dbField": "db",
      "dbField2": "totDataBytes"
    }');
    esPost("/${PREFIX}fields_v2/field/databytes.src", '{
      "friendlyName": "Src data bytes",
      "group": "general",
      "help": "Total number of data bytes sent by source in a session",
      "type": "integer",
      "dbField": "db1",
      "dbField2": "srcDataBytes"
    }');
    esPost("/${PREFIX}fields_v2/field/databytes.dst", '{
      "friendlyName": "Dst data bytes",
      "group": "general",
      "help": "Total number of data bytes sent by destination in a session",
      "type": "integer",
      "dbField": "db2",
      "dbField2": "dstDataBytes"
    }');
    esPost("/${PREFIX}fields_v2/field/packets", '{
      "friendlyName": "Packets",
      "group": "general",
      "help": "Total number of packets sent AND received in a session",
      "type": "integer",
      "dbField": "pa",
      "dbField2": "totPackets"
    }');
    esPost("/${PREFIX}fields_v2/field/packets.src", '{
      "friendlyName": "Src Packets",
      "group": "general",
      "help": "Total number of packets sent by source in a session",
      "type": "integer",
      "dbField": "pa1",
      "dbField2": "srcPackets"
    }');
    esPost("/${PREFIX}fields_v2/field/packets.dst", '{
      "friendlyName": "Dst Packets",
      "group": "general",
      "help": "Total number of packets sent by destination in a session",
      "type": "integer",
      "dbField": "pa2",
      "dbField2": "dstPackets"
    }');
    esPost("/${PREFIX}fields_v2/field/ip.protocol", '{
      "friendlyName": "IP Protocol",
      "group": "general",
      "help": "IP protocol number or friendly name",
      "type": "lotermfield",
      "dbField": "pr",
      "dbField2": "ipProtocol",
      "transform": "ipProtocolLookup"
    }');
    esPost("/${PREFIX}fields_v2/field/id", '{
      "friendlyName": "Moloch ID",
      "group": "general",
      "help": "Moloch ID for the session",
      "type": "termfield",
      "dbField": "_id",
      "dbField2": "_id",
      "noFacet": "true"

    }');
    esPost("/${PREFIX}fields_v2/field/rootId", '{
      "friendlyName": "Moloch Root ID",
      "group": "general",
      "help": "Moloch ID of the first session in a multi session stream",
      "type": "termfield",
      "dbField": "ro",
      "dbField2": "rootId"
    }');
    esPost("/${PREFIX}fields_v2/field/node", '{
      "friendlyName": "Moloch Node",
      "group": "general",
      "help": "Moloch node name the session was recorded on",
      "type": "termfield",
      "dbField": "no",
      "dbField2": "node"
    }');
    esPost("/${PREFIX}fields_v2/field/file", '{
      "friendlyName": "Filename",
      "group": "general",
      "help": "Moloch offline pcap filename",
      "type": "fileand",
      "dbField": "fileand",
      "dbField2": "fileand"
    }');
    esPost("/${PREFIX}fields_v2/field/payload8.src.hex", '{
      "friendlyName": "Payload Src Hex",
      "group": "general",
      "help": "First 8 bytes of source payload in hex",
      "type": "lotermfield",
      "dbField": "fb1",
      "dbField2": "srcPayload8",
      "aliases": ["payload.src"]
    }');
    esPost("/${PREFIX}fields_v2/field/payload8.src.utf8", '{
      "friendlyName": "Payload Src UTF8",
      "group": "general",
      "help": "First 8 bytes of source payload in utf8",
      "type": "termfield",
      "dbField": "fb1",
      "dbField2": "srcPayload8",
      "transform": "utf8ToHex",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v2/field/payload8.dst.hex", '{
      "friendlyName": "Payload Dst Hex",
      "group": "general",
      "help": "First 8 bytes of destination payload in hex",
      "type": "lotermfield",
      "dbField": "fb2",
      "dbField2": "dstPayload8",
      "aliases": ["payload.dst"]
    }');
    esPost("/${PREFIX}fields_v2/field/payload8.dst.utf8", '{
      "friendlyName": "Payload Dst UTF8",
      "group": "general",
      "help": "First 8 bytes of destination payload in utf8",
      "type": "termfield",
      "dbField": "fb2",
      "dbField2": "dstPayload8",
      "transform": "utf8ToHex",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v2/field/payload8.hex", '{
      "friendlyName": "Payload Hex",
      "group": "general",
      "help": "First 8 bytes of payload in hex",
      "type": "lotermfield",
      "dbField": "fballhex",
      "dbField2": "fballhex",
      "regex": "^payload8.(src|dst).hex$"
    }');
    esPost("/${PREFIX}fields_v2/field/payload8.utf8", '{
      "friendlyName": "Payload UTF8",
      "group": "general",
      "help": "First 8 bytes of payload in hex",
      "type": "lotermfield",
      "dbField": "fballutf8",
      "dbField2": "fballutf8",
      "regex": "^payload8.(src|dst).utf8$"
    }');
    esPost("/${PREFIX}fields_v2/field/scrubbed.by", '{
      "friendlyName": "Scrubbed By",
      "group": "general",
      "help": "SPI data was scrubbed by",
      "type": "lotermfield",
      "dbField": "scrubby",
      "dbField2": "scrubby"
    }');
    esPost("/${PREFIX}fields_v2/field/view", '{
      "friendlyName": "View Name",
      "group": "general",
      "help": "Moloch view name",
      "type": "viewand",
      "dbField": "viewand",
      "dbField2": "viewand",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v2/field/starttime", '{
      "friendlyName": "Start Time",
      "group": "general",
      "help": "Session Start Time",
      "type": "seconds",
      "type2": "date",
      "dbField": "fp",
      "dbField2": "firstPacket"
    }');
    esPost("/${PREFIX}fields_v2/field/stoptime", '{
      "friendlyName": "Stop Time",
      "group": "general",
      "help": "Session Stop Time",
      "type": "seconds",
      "type2": "date",
      "dbField": "lp",
      "dbField2": "lastPacket"
    }');
    esPost("/${PREFIX}fields_v2/field/huntId", '{
      "friendlyName": "Hunt ID",
      "group": "general",
      "help": "The ID of the packet search job that matched this session",
      "type": "termfield",
      "dbField": "huntId",
      "dbField2": "huntId"
    }');
    esPost("/${PREFIX}fields_v2/field/huntName", '{
      "friendlyName": "Hunt Name",
      "group": "general",
      "help": "The name of the packet search job that matched this session",
      "type": "termfield",
      "dbField": "huntName",
      "dbField2": "huntName"
    }');
}

################################################################################
sub queriesCreate
{
    my $settings = '
{
  "settings": {
    "index.priority": 40,
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    logmsg "Creating queries index\n" if ($verbose > 0);
    esPut("/${PREFIX}queries_v2", $settings);
    queriesUpdate();
}
################################################################################
sub queriesUpdate
{
    my $mapping = '
{
  "query": {
    "_source": {"enabled": "true"},
    "dynamic": "strict",
    "properties": {
      "name": {
        "type": "keyword"
      },
      "enabled": {
        "type": "boolean"
      },
      "lpValue": {
        "type": "long"
      },
      "lastRun": {
        "type": "date"
      },
      "count": {
        "type": "long"
      },
      "query": {
        "type": "keyword"
      },
      "action": {
        "type": "keyword"
      },
      "creator": {
        "type": "keyword"
      },
      "tags": {
        "type": "keyword"
      },
      "notifier": {
        "type": "keyword"
      },
      "lastNotified": {
        "type": "date"
      },
      "lastNotifiedCount": {
        "type": "long"
      }
    }
  }
}';

    logmsg "Setting queries mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}queries_v2/query/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping);
    esAlias("add", "queries_v2", "queries");
}

################################################################################
sub sessions2Update
{
    my $mapping = '
{
  "session": {
    "_meta": {
      "molochDbVersion": ' . $VERSION . '
    },
    "dynamic": "true",
    "dynamic_templates": [
      {
        "template_ip_end": {
          "match": "*Ip",
          "mapping": {
            "type": "ip"
          }
        }
      },
      {
        "template_ip_alone": {
          "match": "ip",
          "mapping": {
            "type": "ip"
          }
        }
      },
      {
        "template_word_split": {
          "match": "*Tokens",
          "mapping": {
            "analyzer": "wordSplit",
            "type": "text",
            "norms": false
          }
        }
      },
      {
        "template_string": {
          "match_mapping_type": "string",
          "mapping": {
            "type": "keyword"
          }
        }
      }
    ],
    "properties": {
      "timestamp": {
        "type": "date"
      },
      "firstPacket": {
        "type": "date"
      },
      "lastPacket": {
        "type": "date"
      },
      "packetPos": {
        "type": "long",
        "index": false
      },
      "packetLen": {
        "type": "integer",
        "index": false
      },
      "cert": {
        "type": "object",
        "properties": {
          "notBefore": {
            "type": "date"
          },
          "notAfter": {
            "type": "date"
          }
        }
      },
      "dhcp": {
        "type": "object",
        "properties": {
          "host": {
            "type": "keyword",
            "copy_to": "dhcp.hostTokens"
          }
        }
      },
      "dns": {
        "type": "object",
        "properties": {
          "host": {
            "type": "keyword",
            "copy_to": "dns.hostTokens"
          }
        }
      },
      "email": {
        "type": "object",
        "properties": {
          "host": {
            "type": "keyword",
            "copy_to": "email.hostTokens"
          }
        }
      },
      "http": {
        "type": "object",
        "properties": {
          "host": {
            "type": "keyword",
            "copy_to": "http.hostTokens"
          },
          "useragent" : {
            "type" : "keyword",
            "copy_to": "http.useragentTokens"
          },
          "uri" : {
            "type" : "keyword",
            "copy_to": "http.uriTokens"
          }
        }
      },
      "quic": {
        "type": "object",
        "properties": {
          "host": {
            "type": "keyword",
            "copy_to": "quic.hostTokens"
          },
          "useragent" : {
            "type" : "keyword",
            "copy_to": "quic.useragentTokens"
          }
        }
      },
      "smb": {
        "type": "object",
        "properties": {
          "host": {
            "type": "keyword",
            "copy_to": "smb.hostTokens"
          }
        }
      },
      "socks": {
        "type": "object",
        "properties": {
          "host": {
            "type": "keyword",
            "copy_to": "socks.hostTokens"
          }
        }
      },
      "oracle": {
        "type": "object",
        "properties": {
          "host": {
            "type": "keyword",
            "copy_to": "oracle.hostTokens"
          }
        }
      }
    }
  }
}
';

$REPLICAS = 0 if ($REPLICAS < 0);
my $shardsPerNode = ceil($SHARDS * ($REPLICAS+1) / $main::numberOfNodes);
$shardsPerNode = $SHARDSPERNODE if ($SHARDSPERNODE eq "null" || $SHARDSPERNODE > $shardsPerNode);

my $hotwarm = '';
if ($DOHOTWARM) {
  $hotwarm = ',
      "routing.allocation.require.molochtype": "hot"';
}

    my $template = '
{
  "index_patterns": "' . $PREFIX . 'sessions2-*",
  "settings": {
    "index": {
      "routing.allocation.total_shards_per_node": ' . $shardsPerNode . $hotwarm . ',
      "refresh_interval": "60s",
      "number_of_shards": ' . $SHARDS . ',
      "number_of_replicas": ' . $REPLICAS . ',
      "analysis": {
        "analyzer": {
          "wordSplit": {
            "type": "custom",
            "tokenizer": "pattern",
            "filter": ["lowercase"]
          }
        }
      }
    }
  },
  "mappings":' . $mapping . '
}';

    logmsg "Creating sessions template\n" if ($verbose > 0);
    esPut("/_template/${PREFIX}sessions2_template?master_timeout=${ESTIMEOUT}s", $template);

    my $indices = esGet("/${PREFIX}sessions2-*/_alias", 1);

    if ($UPGRADEALLSESSIONS) {
        logmsg "Updating sessions2 mapping for ", scalar(keys %{$indices}), " indices\n" if (scalar(keys %{$indices}) != 0);
        foreach my $i (keys %{$indices}) {
            progress("$i ");
            esPut("/$i/session/_mapping?master_timeout=${ESTIMEOUT}s", $mapping, 1);
        }
        logmsg "\n";
    }
}

################################################################################
sub historyUpdate
{
    my $mapping = '
{
  "history": {
    "_source": {"enabled": "true"},
    "dynamic": "strict",
    "properties": {
      "uiPage": {
        "type": "keyword"
      },
      "userId": {
        "type": "keyword"
      },
      "method": {
        "type": "keyword"
      },
      "api": {
        "type": "keyword"
      },
      "expression": {
        "type": "keyword"
      },
      "view": {
        "type": "object",
        "dynamic": "true"
      },
      "timestamp": {
        "type": "date"
      },
      "range": {
        "type": "integer"
      },
      "query": {
        "type": "keyword"
      },
      "queryTime": {
        "type": "integer"
      },
      "recordsReturned": {
        "type": "integer"
      },
      "recordsFiltered": {
        "type": "long"
      },
      "recordsTotal": {
        "type": "long"
      },
      "body": {
        "type": "object",
        "dynamic": "true"
      },
      "forcedExpression": {
        "type": "keyword"
      }
    }
  }
}';

 my $template = '
{
  "index_patterns": "' . $PREFIX . 'history_v1-*",
  "settings": {
      "number_of_shards": 1,
      "number_of_replicas": 0,
      "auto_expand_replicas": "0-1"
    },
  "mappings":' . $mapping . '
}';

logmsg "Creating history template\n" if ($verbose > 0);
esPut("/_template/${PREFIX}history_v1_template?master_timeout=${ESTIMEOUT}s", $template);

my $indices = esGet("/${PREFIX}history_v1-*/_alias", 1);

logmsg "Updating history mapping for ", scalar(keys %{$indices}), " indices\n" if (scalar(keys %{$indices}) != 0);
foreach my $i (keys %{$indices}) {
    progress("$i ");
    esPut("/$i/history/_mapping?master_timeout=${ESTIMEOUT}s", $mapping, 1);
}

logmsg "\n";
}
################################################################################

################################################################################
sub huntsCreate
{
  my $settings = '
{
  "settings": {
    "index.priority": 30,
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

  logmsg "Creating hunts_v1 index\n" if ($verbose > 0);
  esPut("/${PREFIX}hunts_v1", $settings);
  esAlias("add", "hunts_v1", "hunts");
  huntsUpdate();
}

sub huntsUpdate
{
    my $mapping = '
{
  "hunt": {
    "_source": {"enabled": "true"},
    "dynamic": "strict",
    "properties": {
      "userId": {
        "type": "keyword"
      },
      "status": {
        "type": "keyword"
      },
      "name": {
        "type": "keyword"
      },
      "size": {
        "type": "integer"
      },
      "search": {
        "type": "keyword"
      },
      "searchType": {
        "type": "keyword"
      },
      "src": {
        "type": "boolean"
      },
      "dst": {
        "type": "boolean"
      },
      "type": {
        "type": "keyword"
      },
      "matchedSessions": {
        "type": "integer"
      },
      "searchedSessions": {
        "type": "integer"
      },
      "totalSessions": {
        "type": "integer"
      },
      "lastPacketTime": {
        "type": "date"
      },
      "created": {
        "type": "date"
      },
      "lastUpdated": {
        "type": "date"
      },
      "started": {
        "type": "date"
      },
      "query": {
        "type": "object",
        "dynamic": "true"
      },
      "errors": {
        "properties": {
          "value": {
            "type": "keyword"
          }
        }
      },
      "notifier": {
        "type": "keyword"
      }
    }
  }
}';

logmsg "Setting hunts_v1 mapping\n" if ($verbose > 0);
esPut("/${PREFIX}hunts_v1/hunt/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping);
}
################################################################################

################################################################################
sub usersCreate
{
    my $settings = '
{
  "settings": {
    "index.priority": 60,
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    logmsg "Creating users_v6 index\n" if ($verbose > 0);
    esPut("/${PREFIX}users_v6", $settings);
    esAlias("add", "users_v6", "users");
    usersUpdate();
}
################################################################################
sub usersUpdate
{
    my $mapping = '
{
  "user": {
    "_source": {"enabled": "true"},
    "dynamic": "strict",
    "properties": {
      "userId": {
        "type": "keyword"
      },
      "userName": {
        "type": "keyword"
      },
      "enabled": {
        "type": "boolean"
      },
      "createEnabled": {
        "type": "boolean"
      },
      "webEnabled": {
        "type": "boolean"
      },
      "headerAuthEnabled": {
        "type": "boolean"
      },
      "emailSearch": {
        "type": "boolean"
      },
      "removeEnabled": {
        "type": "boolean"
      },
      "packetSearch": {
        "type": "boolean"
      },
      "hideStats": {
        "type": "boolean"
      },
      "hideFiles": {
        "type": "boolean"
      },
      "hidePcap": {
        "type": "boolean"
      },
      "disablePcapDownload": {
        "type": "boolean"
      },
      "passStore": {
        "type": "keyword"
      },
      "expression": {
        "type": "keyword"
      },
      "settings": {
        "type": "object",
        "dynamic": "true"
      },
      "views": {
        "type": "object",
        "dynamic": "true",
        "enabled": "false"
      },
      "notifiers": {
        "type": "object",
        "dynamic": "true",
        "enabled": "false"
      },
      "columnConfigs": {
        "type": "object",
        "dynamic": "true",
        "enabled": "false"
      },
      "spiviewFieldConfigs": {
        "type": "object",
        "dynamic": "true",
        "enabled": "false"
      },
      "tableStates": {
        "type": "object",
        "dynamic": "true",
        "enabled": "false"
      },
      "welcomeMsgNum": {
        "type": "integer"
      },
      "lastUsed": {
        "type": "date"
      },
      "timeLimit": {
        "type": "integer"
      }
    }
  }
}';

    logmsg "Setting users_v6 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}users_v6/user/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping);
}
################################################################################
sub setPriority
{
    esPut("/${PREFIX}sequence/_settings?master_timeout=${ESTIMEOUT}s", '{"settings": {"index.priority": 100}}', 1);
    esPut("/${PREFIX}fields/_settings?master_timeout=${ESTIMEOUT}s", '{"settings": {"index.priority": 90}}', 1);
    esPut("/${PREFIX}files/_settings?master_timeout=${ESTIMEOUT}s", '{"settings": {"index.priority": 80}}', 1);
    esPut("/${PREFIX}stats/_settings?master_timeout=${ESTIMEOUT}s", '{"settings": {"index.priority": 70}}', 1);
    esPut("/${PREFIX}users/_settings?master_timeout=${ESTIMEOUT}s", '{"settings": {"index.priority": 60}}', 1);
    esPut("/${PREFIX}dstats/_settings?master_timeout=${ESTIMEOUT}s", '{"settings": {"index.priority": 50}}', 1);
    esPut("/${PREFIX}queries/_settings?master_timeout=${ESTIMEOUT}s", '{"settings": {"index.priority": 40}}', 1);
    esPut("/${PREFIX}hunts/_settings?master_timeout=${ESTIMEOUT}s", '{"settings": {"index.priority": 30}}', 1);
}
################################################################################
sub createNewAliasesFromOld
{
    my ($alias, $newName, $oldName, $createFunction) = @_;

    if (esCheckAlias("${PREFIX}$alias", "${PREFIX}$newName") && esIndexExists("${PREFIX}$newName")) {
        logmsg ("SKIPPING - ${PREFIX}$alias already points to ${PREFIX}$newName\n");
        return;
    }

    if (!esIndexExists("${PREFIX}$oldName")) {
        die "ERROR - ${PREFIX}$oldName doesn't exist!";
    }

    $createFunction->();
    esAlias("remove", $oldName, $alias);
    esCopy($oldName, $newName);
    esDelete("/${PREFIX}${oldName}", 1);
}
################################################################################
sub kind2time
{
    my ($kind, $num) = @_;

    my @theTime = gmtime;

    if ($kind eq "hourly") {
        $theTime[2] -= $num;
    } elsif ($kind =~ /^hourly([23468])$/) {
        $theTime[2] -= $num * int($1);
    } elsif ($kind eq "hourly12") {
        $theTime[2] -= $num * 12;
    } elsif ($kind eq "daily") {
        $theTime[3] -= $num;
    } elsif ($kind eq "weekly") {
        $theTime[3] -= 7*$num;
    } elsif ($kind eq "monthly") {
        $theTime[4] -= $num;
    }

    return @theTime;
}
################################################################################
sub mktimegm
{
  local $ENV{TZ} = 'UTC';
  return mktime(@_);
}
################################################################################
sub index2time
{
my($index) = @_;

  return 0 if ($index !~ /sessions2-(.*)$/);
  $index = $1;

  my @t;

  $t[0] = 59;
  $t[1] = 59;
  $t[5] = int (substr($index, 0, 2));
  $t[5] += 100 if ($t[5] < 50);

  if ($index =~ /m/) {
      $t[2] = 23;
      $t[3] = 28;
      $t[4] = int(substr($index, 3, 2)) - 1;
  } elsif ($index =~ /w/) {
      $t[2] = 23;
      $t[3] = int(substr($index, 3, 2)) * 7 + 3;
  } elsif ($index =~ /h/) {
      $t[4] = int(substr($index, 2, 2)) - 1;
      $t[3] = int(substr($index, 4, 2));
      $t[2] = int(substr($index, 7, 2));
  } else {
      $t[2] = 23;
      $t[3] = int(substr($index, 4, 2));
      $t[4] = int(substr($index, 2, 2)) - 1;
  }

  return mktimegm(@t);
}

################################################################################
sub time2index
{
my($type, $prefix, $t) = @_;

    my @t = gmtime($t);
    if ($type eq "hourly") {
        return sprintf("${PREFIX}${prefix}%02d%02d%02dh%02d", $t[5] % 100, $t[4]+1, $t[3], $t[2]);
    }

    if ($type =~ /^hourly([23468])$/) {
        my $n = int($1);
        return sprintf("${PREFIX}${prefix}%02d%02d%02dh%02d", $t[5] % 100, $t[4]+1, $t[3], int($t[2]/$n)*$n);
    }

    if ($type eq "hourly12") {
        return sprintf("${PREFIX}${prefix}%02d%02d%02dh%02d", $t[5] % 100, $t[4]+1, $t[3], int($t[2]/12)*12);
    }

    if ($type eq "daily") {
        return sprintf("${PREFIX}${prefix}%02d%02d%02d", $t[5] % 100, $t[4]+1, $t[3]);
    }

    if ($type eq "weekly") {
        return sprintf("${PREFIX}${prefix}%02dw%02d", $t[5] % 100, int($t[7]/7));
    }

    if ($type eq "monthly") {
        return sprintf("${PREFIX}${prefix}%02dm%02d", $t[5] % 100, $t[4]+1);
    }
}

################################################################################
sub dbESVersion {
    my $esversion = esGet("/");
    my @parts = split(/\./, $esversion->{version}->{number});
    $main::esVersion = int($parts[0]*100*100) + int($parts[1]*100) + int($parts[2]);
    return $esversion;
}
################################################################################
sub dbVersion {
my ($loud) = @_;
    my $version;

    $version = esGet("/_template/${PREFIX}sessions2_template?filter_path=**._meta", 1);

    if (defined $version &&
        exists $version->{"${PREFIX}sessions2_template"} &&
        exists $version->{"${PREFIX}sessions2_template"}->{mappings}->{session} &&
        exists $version->{"${PREFIX}sessions2_template"}->{mappings}->{session}->{_meta} &&
        exists $version->{"${PREFIX}sessions2_template"}->{mappings}->{session}->{_meta}->{molochDbVersion}
    ) {
        $main::versionNumber = $version->{"${PREFIX}sessions2_template"}->{mappings}->{session}->{_meta}->{molochDbVersion};
        return;
    }

    my $version = esGet("/${PREFIX}dstats/version/version", 1);

    my $found = $version->{found};

    if (!defined $found) {
        logmsg "This is a fresh Moloch install\n" if ($loud);
        $main::versionNumber = -1;
        if ($loud && $ARGV[1] !~ "init") {
            die "Looks like moloch wasn't installed, must do init"
        }
    } elsif ($found == 0) {
        $main::versionNumber = 0;
    } else {
        $main::versionNumber = $version->{_source}->{version};
    }
}
################################################################################
sub dbCheckForActivity {
    logmsg "This upgrade requires all capture nodes to be stopped.  Checking\n";
    my $json1 = esGet("/${PREFIX}stats/stat/_search?size=1000");
    sleep(6);
    my $json2 = esGet("/${PREFIX}stats/stat/_search?size=1000");
    die "Some capture nodes still active" if ($json1->{hits}->{total} != $json2->{hits}->{total});
    return if ($json1->{hits}->{total} == 0);

    my @hits1 = sort {$a->{_source}->{nodeName} cmp $b->{_source}->{nodeName}} @{$json1->{hits}->{hits}};
    my @hits2 = sort {$a->{_source}->{nodeName} cmp $b->{_source}->{nodeName}} @{$json2->{hits}->{hits}};

    for (my $i = 0; $i < $json1->{hits}->{total}; $i++) {
        if ($hits1[$i]->{_source}->{nodeName} ne $hits2[$i]->{_source}->{nodeName}) {
            die "Capture node '" . $hits1[$i]->{_source}->{nodeName} . "' or '" . $hits2[$i]->{_source}->{nodeName} . "' still active";
        }

        if ($hits1[$i]->{_source}->{currentTime} != $hits2[$i]->{_source}->{currentTime}) {
            die "Capture node '" . $hits1[$i]->{_source}->{nodeName} . "' still active";
        }
    }
}
################################################################################
sub dbCheckHealth {
    my $health = esGet("/_cluster/health");
    if ($health->{status} ne "green") {
        logmsg("WARNING elasticsearch health is '$health->{status}' instead of 'green', things may be broken\n\n");
    }
    return $health;
}
################################################################################
sub dbCheck {
    my $esversion = dbESVersion();
    my @parts = split(/\./, $esversion->{version}->{number});
    $main::esVersion = int($parts[0]*100*100) + int($parts[1]*100) + int($parts[2]);

    if ($main::esVersion < 60600 ||
        $main::esVersion >= 70000)
    {
        logmsg("Currently using Elasticsearch version ", $esversion->{version}->{number}, " which isn't supported\n",
              "* < 6.6.0 are not supported\n",
              "* > 7.x are not supported\n",
              "\n",
              "Instructions: https://molo.ch/faq#how-do-i-upgrade-elasticsearch\n",
              "Make sure to restart any viewer or capture after upgrading!\n"
             );
        exit (1);
    }

    my $error = 0;
    my $nodes = esGet("/_nodes?flat_settings");
    my $nodeStats = esGet("/_nodes/stats");

    foreach my $key (sort {$nodes->{nodes}->{$a}->{name} cmp $nodes->{nodes}->{$b}->{name}} keys %{$nodes->{nodes}}) {
        next if (exists $nodes->{$key}->{attributes} && exists $nodes->{$key}->{attributes}->{data} && $nodes->{$key}->{attributes}->{data} eq "false");
        my $node = $nodes->{nodes}->{$key};
        my $nodeStat = $nodeStats->{nodes}->{$key};
        my $errstr;
        my $warnstr;

        if (exists $node->{settings}->{"index.cache.field.type"}) {
            $errstr .= sprintf ("    REMOVE 'index.cache.field.type'\n");
        }

        if (!(exists $nodeStat->{process}->{max_file_descriptors}) || int($nodeStat->{process}->{max_file_descriptors}) < 4000) {
            $errstr .= sprintf ("  INCREASE max file descriptors in /etc/security/limits.conf and restart all ES node\n");
            $errstr .= sprintf ("                (change root to the user that runs ES)\n");
            $errstr .= sprintf ("          root hard nofile 128000\n");
            $errstr .= sprintf ("          root soft nofile 128000\n");
        }

        if ($errstr) {
            $error = 1;
            logmsg ("\nERROR: On node ", $node->{name}, " machine ", ($node->{hostname} || $node->{host}), " in file ", $node->{settings}->{config}, "\n");
            logmsg($errstr);
        }

        if ($warnstr) {
            logmsg ("\nWARNING: On node ", $node->{name}, " machine ", ($node->{hostname} || $node->{host}), " in file ", $node->{settings}->{config}, "\n");
            logmsg($warnstr);
        }
    }

    if ($error) {
        logmsg "\nFix above errors before proceeding\n";
        exit (1);
    }
}
################################################################################
sub checkForOld2Indices {
    my $result = esGet("/_all/_settings/index.version.created?pretty");
    my $found = 0;

    while ( my ($key, $value) = each (%{$result})) {
        if ($value->{settings}->{index}->{version}->{created} < 2000000) {
            logmsg "WARNING: You must delete index '$key' before upgrading to ES 5\n";
            $found = 1;
        }
    }

    if ($found) {
        logmsg "\nYou MUST delete (and optionally re-add) the indices above while still on ES 2.x otherwise ES 5.x will NOT start.\n\n";
    }
}
################################################################################
sub checkForOld5Indices {
    my $result = esGet("/_all/_settings/index.version.created?pretty");
    my $found = 0;

    while ( my ($key, $value) = each (%{$result})) {
        if ($value->{settings}->{index}->{version}->{created} < 5000000) {
            logmsg "WARNING: You must delete index '$key' before upgrading to ES 6\n";
            $found = 1;
        }
    }

    if ($found) {
        logmsg "\nYou MUST delete (and optionally re-add) the indices above while still on ES 5.x otherwise ES 6.x will NOT start.\n\n";
    }
}
################################################################################
sub progress {
    my ($msg) = @_;
    if ($verbose == 1) {
        local $| = 1;
        logmsg ".";
    } elsif ($verbose == 2) {
        local $| = 1;
        logmsg "$msg";
    }
}
################################################################################
sub optimizeOther {
    logmsg "Optimizing Admin Indices\n";
    esForceMerge("${PREFIX}stats_v3,${PREFIX}dstats_v3,${PREFIX}files_v5,${PREFIX}sequence_v2,${PREFIX}users_v6,${PREFIX}queries_v2,${PREFIX}hunts_v1", 1);
    logmsg "\n" if ($verbose > 0);
}
################################################################################
sub parseArgs {
    my ($pos) = @_;

    for (;$pos <= $#ARGV; $pos++) {
        if ($ARGV[$pos] eq "--shards") {
            $pos++;
            $SHARDS = $ARGV[$pos];
        } elsif ($ARGV[$pos] eq "--replicas") {
            $pos++;
            $REPLICAS = int($ARGV[$pos]);
        } elsif ($ARGV[$pos] eq "--history") {
            $pos++;
            $HISTORY = int($ARGV[$pos]);
        } elsif ($ARGV[$pos] eq "--segments") {
            $pos++;
            $SEGMENTS = int($ARGV[$pos]);
        } elsif ($ARGV[$pos] eq "--nooptimize") {
	    $NOOPTIMIZE = 1;
        } elsif ($ARGV[$pos] eq "--full") {
	    $FULL = 1;
        } elsif ($ARGV[$pos] eq "--reverse") {
	    $REVERSE = 1;
        } elsif ($ARGV[$pos] eq "--skipupgradeall") {
            $UPGRADEALLSESSIONS = 0;
        } elsif ($ARGV[$pos] eq "--shardsPerNode") {
            $pos++;
            if ($ARGV[$pos] eq "null") {
                $SHARDSPERNODE = "null";
            } else {
                $SHARDSPERNODE = int($ARGV[$pos]);
            }
        } elsif ($ARGV[$pos] eq "--hotwarm") {
            $DOHOTWARM = 1;
        } elsif ($ARGV[$pos] eq "--warmafter") {
            $pos++;
            $WARMAFTER = int($ARGV[$pos]);
        } else {
            logmsg "Unknown option '$ARGV[$pos]'\n";
        }
    }
}
################################################################################
while (@ARGV > 0 && substr($ARGV[0], 0, 1) eq "-") {
    if ($ARGV[0] =~ /(-v+|--verbose)$/) {
         $verbose += ($ARGV[0] =~ tr/v//);
    } elsif ($ARGV[0] =~ /--prefix$/) {
        $PREFIX = $ARGV[1];
        shift @ARGV;
        $PREFIX .= "_" if ($PREFIX !~ /_$/);
    } elsif ($ARGV[0] =~ /-n$/) {
        $NOCHANGES = 1;
    } elsif ($ARGV[0] =~ /--timeout$/) {
        $ESTIMEOUT = int($ARGV[1]);
        shift @ARGV;
    } else {
        showHelp("Unknkown global option $ARGV[0]")
    }
    shift @ARGV;
}

showHelp("Help:") if ($ARGV[1] =~ /^help$/);
showHelp("Missing arguments") if (@ARGV < 2);
showHelp("Unknown command '$ARGV[1]'") if ($ARGV[1] !~ /^(init|initnoprompt|clean|info|wipe|upgrade|upgradenoprompt|disableusers|users-?import|import|restore|users-?export|export|backup|expire|rotate|optimize|mv|rm|rm-?missing|rm-?node|add-?missing|field|force-?put-?version|sync-?files|hide-?node|unhide-?node|add-?alias|set-?replicas|set-?shards-?per-?node|set-?allocation-?enable|allocate-?empty|unflood-?stage)$/);
showHelp("Missing arguments") if (@ARGV < 3 && $ARGV[1] =~ /^(users-?import|import|users-?export|backup|restore|rm|rm-?missing|rm-?node|hide-?node|unhide-?node|set-?allocation-?enable|unflood-?stage)$/);
showHelp("Missing arguments") if (@ARGV < 4 && $ARGV[1] =~ /^(field|export|add-?missing|sync-?files|add-?alias|set-?replicas|set-?shards-?per-?node)$/);
showHelp("Missing arguments") if (@ARGV < 5 && $ARGV[1] =~ /^(allocate-?empty)$/);
showHelp("Must have both <old fn> and <new fn>") if (@ARGV < 4 && $ARGV[1] =~ /^(mv)$/);
showHelp("Must have both <type> and <num> arguments") if (@ARGV < 4 && $ARGV[1] =~ /^(rotate|expire)$/);

parseArgs(2) if ($ARGV[1] =~ /^(init|initnoprompt|upgrade|upgradenoprompt|clean)$/);
parseArgs(3) if ($ARGV[1] =~ /^(restore)$/);

$main::userAgent = LWP::UserAgent->new(timeout => $ESTIMEOUT + 5, keep_alive => 5);

if ($ARGV[0] =~ /^http/) {
    $main::elasticsearch = $ARGV[0];
} else {
    $main::elasticsearch = "http://$ARGV[0]";
}

if ($ARGV[1] =~ /^(users-?import|import)$/) {
    open(my $fh, "<", $ARGV[2]) or die "cannot open < $ARGV[2]: $!";
    my $data = do { local $/; <$fh> };
    esPost("/_bulk", $data);
    close($fh);
    exit 0;
} elsif ($ARGV[1] =~ /^export$/) {
    my $index = $ARGV[2];
    my $data = esScroll($index, "", '{"version": true}');
    if (scalar(@{$data}) == 0){
        logmsg "The index is empty\n";
        exit 0;
    }
    open(my $fh, ">", "$ARGV[3].${PREFIX}${index}.json") or die "cannot open > $ARGV[3].${PREFIX}${index}.json: $!";
    foreach my $hit (@{$data}) {
        print $fh "{\"index\": {\"_index\": \"${PREFIX}${index}\", \"_type\": \"$hit->{_type}\", \"_id\": \"$hit->{_id}\", \"_version\": $hit->{_version}, \"_version_type\": \"external\"}}\n";
        if (exists $hit->{_source}) {
            print $fh to_json($hit->{_source}) . "\n";
        } else {
            print $fh "{}\n";
        }
    }
    close($fh);
    exit 0;
} elsif ($ARGV[1] =~ /^backup$/) {
    my @indexes = ("users", "sequence", "stats", "queries", "files", "fields", "dstats");
    push(@indexes, "hunts") if ($main::versionNumber > 51);
    logmsg "Exporting documents...\n";
    foreach my $index (@indexes) {
        my $data = esScroll($index, "", '{"version": true}');
        next if (scalar(@{$data}) == 0);
        open(my $fh, ">", "$ARGV[2].${PREFIX}${index}.json") or die "cannot open > $ARGV[2].${PREFIX}${index}.json: $!";
        foreach my $hit (@{$data}) {
            print $fh "{\"index\": {\"_index\": \"${PREFIX}${index}\", \"_type\": \"$hit->{_type}\", \"_id\": \"$hit->{_id}\", \"_version\": $hit->{_version}, \"_version_type\": \"external\"}}\n";
            if (exists $hit->{_source}) {
                print $fh to_json($hit->{_source}) . "\n";
            } else {
                print $fh "{}\n";
            }
        }
        close($fh);
    }
    logmsg "Exporting templates...\n";
    my @templates = ("sessions2_template", "history_v1_template");
    foreach my $template (@templates) {
        my $data = esGet("/_template/${PREFIX}${template}");
        my @name = split(/_/, $template);
        open(my $fh, ">", "$ARGV[2].${PREFIX}$name[0].template.json") or die "cannot open > $ARGV[2].${PREFIX}$name[0].template.json: $!";
        print $fh to_json($data);
        close($fh);
    }
    logmsg "Exporting settings...\n";
    foreach my $index (@indexes) {
        my $data = esGet("/${PREFIX}${index}/_settings");
        open(my $fh, ">", "$ARGV[2].${PREFIX}${index}.settings.json") or die "cannot open > $ARGV[2].${PREFIX}${index}.settings.json: $!";
        print $fh to_json($data);
        close($fh);
    }
    logmsg "Exporting mappings...\n";
    foreach my $index (@indexes) {
        my $data = esGet("/${PREFIX}${index}/_mappings");
        open(my $fh, ">", "$ARGV[2].${PREFIX}${index}.mappings.json") or die "cannot open > $ARGV[2].${PREFIX}${index}.mappings.json: $!";
        print $fh to_json($data);
        close($fh);
    }
    logmsg "Exporting aliaes...\n";

    my @indexes_prefixed = ();
    foreach my $index (@indexes) {
        push(@indexes_prefixed, $PREFIX . $index);
    }
    my $aliases = join(',', @indexes_prefixed);
    $aliases = "/_cat/aliases/${aliases}?format=json";
    my $data = esGet($aliases), "\n";
    open(my $fh, ">", "$ARGV[2].${PREFIX}aliases.json") or die "cannot open > $ARGV[2].${PREFIX}aliases.json: $!";
    print $fh to_json($data);
    close($fh);
    logmsg "Finished\n";
    exit 0;
} elsif ($ARGV[1] =~ /^users-?export$/) {
    open(my $fh, ">", $ARGV[2]) or die "cannot open > $ARGV[2]: $!";
    my $users = esGet("/${PREFIX}users/_search?size=1000");
    foreach my $hit (@{$users->{hits}->{hits}}) {
        print $fh "{\"index\": {\"_index\": \"users\", \"_type\": \"user\", \"_id\": \"" . $hit->{_id} . "\"}}\n";
        print $fh to_json($hit->{_source}) . "\n";
    }
    close($fh);
    exit 0;
} elsif ($ARGV[1] =~ /^(rotate|expire)$/) {
    showHelp("Invalid expire <type>") if ($ARGV[2] !~ /^(hourly|hourly[23468]|hourly12|daily|weekly|monthly)$/);

    # First handle sessions expire
    my $indices = esGet("/${PREFIX}sessions2-*/_alias", 1);

    my $endTime = time();
    my $endTimeIndex = time2index($ARGV[2], "sessions2-", $endTime);
    delete $indices->{$endTimeIndex}; # Don't optimize current index

    my @startTime = kind2time($ARGV[2], int($ARGV[3]));

    parseArgs(4);

    my $startTime = mktimegm(@startTime);
    my @warmTime = kind2time($ARGV[2], $WARMAFTER);
    my $warmTime = mktimegm(@warmTime);
    my $optimizecnt = 0;
    my $warmcnt = 0;
    my @indiceskeys = sort (keys %{$indices});

    foreach my $i (@indiceskeys) {
        my $t = index2time($i);
        if ($t >= $startTime) {
            $indices->{$i}->{OPTIMIZEIT} = 1;
            $optimizecnt++;
        }
        if ($WARMAFTER != -1 && $t < $warmTime) {
            $indices->{$i}->{WARMIT} = 1;
            $warmcnt++;
        }
    }

    my $nodes = esGet("/_nodes");
    $main::numberOfNodes = dataNodes($nodes->{nodes});
    my $shardsPerNode = ceil($SHARDS * ($REPLICAS+1) / $main::numberOfNodes);
    $shardsPerNode = $SHARDSPERNODE if ($SHARDSPERNODE eq "null" || $SHARDSPERNODE > $shardsPerNode);

    dbESVersion();
    optimizeOther() unless $NOOPTIMIZE ;
    logmsg sprintf ("Expiring %s sessions indices, %s optimizing %s, warming %s\n", commify(scalar(keys %{$indices}) - $optimizecnt), $NOOPTIMIZE?"Not":"", commify($optimizecnt), commify($warmcnt));
    esPost("/_flush/synced", "", 1);

    @indiceskeys = reverse(@indiceskeys) if ($REVERSE);

    # Get all the settings at once, we use below to see if we need to change them
    my $settings = esGet("/_settings?flat_settings&master_timeout=${ESTIMEOUT}s", 1);

    # Find all the shards that have too many segments and increment the OPTIMIZEIT count or not warm
    my $shards = esGet("/_cat/shards?h=i,sc&format=json");
    for my $i (@{$shards}) {
        if (exists $indices->{$i->{i}}->{OPTIMIZEIT} && defined $i->{sc} & int($i->{sc}) > $SEGMENTS) {
            $indices->{$i->{i}}->{OPTIMIZEIT}++;
        }

        if (exists $indices->{$i->{i}}->{WARMIT} && $settings->{$i->{i}}->{settings}->{"index.routing.allocation.require.molochtype"} ne "warm") {
            $indices->{$i->{i}}->{WARMIT}++;
        }
    }

    foreach my $i (@indiceskeys) {
        progress("$i ");
        if (exists $indices->{$i}->{OPTIMIZEIT}) {

            # 1 is set if it shouldn't be expired, > 1 means it needs to be optimized
            if ($indices->{$i}->{OPTIMIZEIT} > 1) {
                esForceMerge($i, $SEGMENTS) unless $NOOPTIMIZE;
            }

            if ($REPLICAS != -1) {
                if (!exists $settings->{$i} ||
                    $settings->{$i}->{settings}->{"index.number_of_replicas"} ne "$REPLICAS" ||
                    ("$shardsPerNode" eq "null" && exists $settings->{$i}->{settings}->{"index.routing.allocation.total_shards_per_node"}) ||
                    ("$shardsPerNode" ne "null" && $settings->{$i}->{settings}->{"index.routing.allocation.total_shards_per_node"} ne "$shardsPerNode")) {

                    esPut("/$i/_settings?master_timeout=${ESTIMEOUT}s", '{"index": {"number_of_replicas":' . $REPLICAS . ', "routing.allocation.total_shards_per_node": ' . $shardsPerNode . '}}', 1);
                }
            }
        } else {
            esDelete("/$i", 1);
        }

        if ($indices->{$i}->{WARMIT} > 1) {
            esPut("/$i/_settings?master_timeout=${ESTIMEOUT}s", '{"index": {"routing.allocation.require.molochtype": "warm"}}', 1);
        }
    }
    esPost("/_flush/synced", "", 1);

    # Now figure out history expire
    my $hindices = esGet("/${PREFIX}history_v1-*/_alias", 1);

    $endTimeIndex = time2index("weekly", "history_v1-", $endTime);
    delete $hindices->{$endTimeIndex};

    @startTime = gmtime;
    $startTime[3] -= 7 * $HISTORY;

    $optimizecnt = 0;
    $startTime = mktimegm(@startTime);
    while ($startTime <= $endTime) {
        my $iname = time2index("weekly", "history_v1-", $startTime);
        if (exists $hindices->{$iname} && $hindices->{$iname}->{OPTIMIZEIT} != 1) {
            $hindices->{$iname}->{OPTIMIZEIT} = 1;
            $optimizecnt++;
        }
        $startTime += 24*60*60;
    }

    logmsg sprintf ("Expiring %s history indices, %s optimizing %s\n", commify(scalar(keys %{$hindices}) - $optimizecnt), $NOOPTIMIZE?"Not":"", commify($optimizecnt));
    foreach my $i (sort (keys %{$hindices})) {
        progress("$i ");
        if (! exists $hindices->{$i}->{OPTIMIZEIT}) {
            esDelete("/$i", 1);
        }
    }
    esForceMerge("${PREFIX}history_*", 1) unless $NOOPTIMIZE;
    esPost("/_flush/synced", "", 1);

    # Give the cluster a kick to rebalance
    esPost("/_cluster/reroute?master_timeout=${ESTIMEOUT}s&retry_failed");
    exit 0;
} elsif ($ARGV[1] eq "optimize") {
    my $indices = esGet("/${PREFIX}sessions2-*/_alias", 1);

    dbESVersion();
    $main::userAgent->timeout(7200);
    esPost("/_flush/synced", "", 1);
    optimizeOther();
    logmsg sprintf "Optimizing %s Session Indices\n", commify(scalar(keys %{$indices}));
    foreach my $i (sort (keys %{$indices})) {
        progress("$i ");
        esForceMerge($i, $SEGMENTS);
    }
    esPost("/_flush/synced", "", 1);
    logmsg "\n";
    exit 0;
} elsif ($ARGV[1] eq "disableusers") {
    showHelp("Invalid number of <days>") if (!defined $ARGV[2] || $ARGV[2] !~ /^[+-]?\d+$/);

    my $users = esGet("/${PREFIX}users/_search?size=1000&q=enabled:true+AND+createEnabled:false+AND+_exists_:lastUsed");
    my $rmcount = 0;

    foreach my $hit (@{$users->{hits}->{hits}}) {
        my $epoc = time();
        my $lastUsed = $hit->{_source}->{lastUsed};
        $lastUsed = $lastUsed / 1000;  # convert to seconds
        $lastUsed = $epoc - $lastUsed; # in seconds
        $lastUsed = $lastUsed / 86400; # days since last used
        if ($lastUsed > $ARGV[2]) {
            my $userId = $hit->{_source}->{userId};
            print "Disabling user: $userId\n";
            esPost("/${PREFIX}users/user/$userId/_update", '{"doc": {"enabled": false}}');
            $rmcount++;
        }
    }

    if ($rmcount == 0) {
      print "No users disabled\n";
    } else {
      print "$rmcount user(s) disabled\n";
    }

    exit 0;
} elsif ($ARGV[1] eq "info") {
    dbVersion(0);
    my $esversion = dbESVersion();
    my $nodes = esGet("/_nodes");
    my $status = esGet("/_stats/docs,store", 1);

    my $sessions = 0;
    my $sessionsBytes = 0;
    my @sessions = grep /^${PREFIX}sessions2-/, keys %{$status->{indices}};
    foreach my $index (@sessions) {
        next if ($index !~ /^${PREFIX}sessions2-/);
        $sessions += $status->{indices}->{$index}->{primaries}->{docs}->{count};
        $sessionsBytes += $status->{indices}->{$index}->{primaries}->{store}->{size_in_bytes};
    }

    my $historys = 0;
    my $historysBytes = 0;
    my @historys = grep /^${PREFIX}history_v1-/, keys %{$status->{indices}};
    foreach my $index (@historys) {
        next if ($index !~ /^${PREFIX}history_v1-/);
        $historys += $status->{indices}->{$index}->{primaries}->{docs}->{count};
        $historysBytes += $status->{indices}->{$index}->{primaries}->{store}->{size_in_bytes};
    }

    sub printIndex {
        my ($status, $name) = @_;
        my $index = $status->{indices}->{$PREFIX.$name};
        return if (!$index);
        printf "%-20s %17s (%s bytes)\n", $name . ":", commify($index->{primaries}->{docs}->{count}), commify($index->{primaries}->{store}->{size_in_bytes});
    }

    printf "ES Version:          %17s\n", $esversion->{version}->{number};
    printf "DB Version:          %17s\n", $main::versionNumber;
    printf "ES Nodes:            %17s/%s\n", commify(dataNodes($nodes->{nodes})), commify(scalar(keys %{$nodes->{nodes}}));
    printf "Session Indices:     %17s\n", commify(scalar(@sessions));
    printf "Sessions2:           %17s (%s bytes)\n", commify($sessions), commify($sessionsBytes);
    if (scalar(@sessions) > 0) {
        printf "Session Density:     %17s (%s bytes)\n", commify(int($sessions/(scalar(keys %{$nodes->{nodes}})*scalar(@sessions)))),
                                                       commify(int($sessionsBytes/(scalar(keys %{$nodes->{nodes}})*scalar(@sessions))));
    }
    printf "History Indices:     %17s\n", commify(scalar(@historys));
    printf "Histories:           %17s (%s bytes)\n", commify($historys), commify($historysBytes);
    if (scalar(@historys) > 0) {
        printf "History Density:     %17s (%s bytes)\n", commify(int($historys/(scalar(keys %{$nodes->{nodes}})*scalar(@historys)))),
                                                       commify(int($historysBytes/(scalar(keys %{$nodes->{nodes}})*scalar(@historys))));
    }
    printIndex($status, "stats_v3");
    printIndex($status, "stats_v2");
    printIndex($status, "files_v5");
    printIndex($status, "files_v4");
    printIndex($status, "users_v6");
    printIndex($status, "users_v5");
    printIndex($status, "users_v4");
    printIndex($status, "hunts_v1");
    printIndex($status, "dstats_v3");
    printIndex($status, "dstats_v2");
    printIndex($status, "sequence_v2");
    printIndex($status, "sequence_v1");
    exit 0;
} elsif ($ARGV[1] eq "mv") {
    (my $fn = $ARGV[2]) =~ s/\//\\\//g;
    my $results = esGet("/${PREFIX}files/_search?q=name:$fn");
    die "Couldn't find '$ARGV[2]' in db\n" if (@{$results->{hits}->{hits}} == 0);

    foreach my $hit (@{$results->{hits}->{hits}}) {
        my $script = '{"script" : "ctx._source.name = \"' . $ARGV[3] . '\"; ctx._source.locked = 1;"}';
        esPost("/${PREFIX}files/file/" . $hit->{_id} . "/_update", $script);
    }
    logmsg "Moved " . scalar (@{$results->{hits}->{hits}}) . " file(s) in database\n";
    exit 0;
} elsif ($ARGV[1] eq "rm") {
    (my $fn = $ARGV[2]) =~ s/\//\\\//g;
    my $results = esGet("/${PREFIX}files/_search?q=name:$fn");
    die "Couldn't find '$ARGV[2]' in db\n" if (@{$results->{hits}->{hits}} == 0);

    foreach my $hit (@{$results->{hits}->{hits}}) {
        esDelete("/${PREFIX}files/file/" . $hit->{_id}, 0);
    }
    logmsg "Removed " . scalar (@{$results->{hits}->{hits}}) . " file(s) in database\n";
    exit 0;
} elsif ($ARGV[1] =~ /^rm-?missing$/) {
    my $results = esGet("/${PREFIX}files/_search?size=10000&q=node:$ARGV[2]");
    die "Couldn't find '$ARGV[2]' in db\n" if (@{$results->{hits}->{hits}} == 0);
    logmsg "Need to remove references to these files from database:\n";
    my $cnt = 0;
    foreach my $hit (@{$results->{hits}->{hits}}) {
        if (! -f $hit->{_source}->{name}) {
            logmsg $hit->{_source}->{name}, "\n";
            $cnt++;
        }
    }
    die "Nothing found to remove." if ($cnt == 0);
    logmsg "\n";
    waitFor("YES", "Do you want to remove file references from database?");
    foreach my $hit (@{$results->{hits}->{hits}}) {
        if (! -f $hit->{_source}->{name}) {
            esDelete("/${PREFIX}files/file/" . $hit->{_id}, 0);
        }
    }
    exit 0;
} elsif ($ARGV[1] =~ /^rm-?node$/) {
    my $results = esGet("/${PREFIX}files/_search?size=10000&q=node:$ARGV[2]");
    logmsg "Deleting ", $results->{hits}->{total}, " files\n";
    foreach my $hit (@{$results->{hits}->{hits}}) {
        esDelete("/${PREFIX}files/file/" . $hit->{_id}, 0);
    }
    esDelete("/${PREFIX}stats/stat/" . $ARGV[2], 1);
    my $results = esGet("/${PREFIX}dstats/_search?size=10000&q=nodeName:$ARGV[2]");
    logmsg "Deleting ", $results->{hits}->{total}, " stats\n";
    foreach my $hit (@{$results->{hits}->{hits}}) {
        esDelete("/${PREFIX}dstats/dstat/" . $hit->{_id}, 0);
    }
    exit 0;
} elsif ($ARGV[1] =~ /^hide-?node$/) {
    my $results = esGet("/${PREFIX}stats/stat/$ARGV[2]", 1);
    die "Node $ARGV[2] not found" if (!$results->{found});
    esPost("/${PREFIX}stats/stat/$ARGV[2]/_update", '{"doc": {"hide": true}}');
    exit 0;
} elsif ($ARGV[1] =~ /^unhide-?node$/) {
    my $results = esGet("/${PREFIX}stats/stat/$ARGV[2]", 1);
    die "Node $ARGV[2] not found" if (!$results->{found});
    esPost("/${PREFIX}stats/stat/$ARGV[2]/_update", '{"script" : "ctx._source.remove(\"hide\")"}');
    exit 0;
} elsif ($ARGV[1] =~ /^add-?alias$/) {
    my $results = esGet("/${PREFIX}stats/stat/$ARGV[2]", 1);
    die "Node $ARGV[2] already exists, must remove first" if ($results->{found});
    esPost("/${PREFIX}stats/stat/$ARGV[2]", '{"nodeName": "' . $ARGV[2] . '", "hostname": "' . $ARGV[3] . '", "hide": true}');
    exit 0;
} elsif ($ARGV[1] =~ /^add-?missing$/) {
    my $dir = $ARGV[3];
    chop $dir if (substr($dir, -1) eq "/");
    opendir(my $dh, $dir) || die "Can't opendir $dir: $!";
    my @files = grep { m/^$ARGV[2]-/ && -f "$dir/$_" } readdir($dh);
    closedir $dh;
    logmsg "Checking ", scalar @files, " files, this may take a while.\n";
    foreach my $file (@files) {
        $file =~ /(\d+)-(\d+).pcap/;
        my $filenum = int($2);
        my $ctime = (stat("$dir/$file"))[10];
        my $info = esGet("/${PREFIX}files/file/$ARGV[2]-$filenum", 1);
        if (!$info->{found}) {
            logmsg "Adding $dir/$file $filenum $ctime\n";
            esPost("/${PREFIX}files/file/$ARGV[2]-$filenum", to_json({
                         'locked' => 0,
                         'first' => $ctime,
                         'num' => $filenum,
                         'name' => "$dir/$file",
                         'node' => $ARGV[2]}), 1);
        } elsif ($verbose > 0) {
            logmsg "Ok $dir/$file\n";
        }
    }
    exit 0;
} elsif ($ARGV[1] =~ /^sync-?files$/) {
    my @nodes = split(",", $ARGV[2]);
    my @dirs = split(",", $ARGV[3]);

    # find all local files, do this first also to make sure we can access dirs
    my @localfiles = ();
    foreach my $dir (@dirs) {
        chop $dir if (substr($dir, -1) eq "/");
        opendir(my $dh, $dir) || die "Can't opendir $dir: $!";
        foreach my $node (@nodes) {
            my @files = grep { m/^$ARGV[2]-/ && -f "$dir/$_" } readdir($dh);
            @files = map "$dir/$_", @files;
            push (@localfiles, @files);
        }
        closedir $dh;
    }

    # See what files are in db
    my $remotefiles = esScroll("files", "file", to_json({'query' => {'terms' => {'node' => \@nodes}}}));
    logmsg("\n") if ($verbose > 0);
    my %remotefileshash;
    foreach my $hit (@{$remotefiles}) {
        if (! -f $hit->{_source}->{name}) {
            progress("Removing " . $hit->{_source}->{name} . " id: " . $hit->{_id} . "\n");
            esDelete("/${PREFIX}files/file/" . $hit->{_id}, 1);
        } else {
            $remotefileshash{$hit->{_source}->{name}} = $hit->{_source};
        }
    }

    # Now see which local are missing
    foreach my $file (@localfiles) {
        my @stat = stat("$file");
        if (!exists $remotefileshash{$file}) {
            $file =~ /\/([^\/]*)-(\d+)-(\d+).pcap/;
            my $node = $1;
            my $filenum = int($3);
            progress("Adding $file $node $filenum $stat[7]\n");
            esPost("/${PREFIX}files/file/$node-$filenum", to_json({
                         'locked' => 0,
                         'first' => $stat[10],
                         'num' => $filenum,
                         'name' => "$file",
                         'node' => $node,
                         'filesize' => $stat[7]}), 1);
        } elsif ($stat[7] != $remotefileshash{$file}->{filesize}) {
          progress("Updating filesize $file $stat[7]\n");
          $file =~ /\/([^\/]*)-(\d+)-(\d+).pcap/;
          my $node = $1;
          my $filenum = int($3);
          $remotefileshash{$file}->{filesize} = $stat[7];
          esPost("/${PREFIX}files/file/$node-$filenum", to_json($remotefileshash{$file}), 1);
        }
    }
    logmsg("\n") if ($verbose > 0);
    exit 0;
} elsif ($ARGV[1] =~ /^(field)$/) {
    my $result = esGet("/${PREFIX}fields/field/$ARGV[3]", 1);
    my $found = $result->{found};
    die "Field $ARGV[3] isn't found" if (!$found);

    esPost("/${PREFIX}fields/field/$ARGV[3]/_update", "{\"doc\":{\"disabled\":" . ($ARGV[2] eq "disable"?"true":"false").  "}}");
    exit 0;
} elsif ($ARGV[1] =~ /^force-?put-?version$/) {
    die "This command doesn't work anymore";
    exit 0;
} elsif ($ARGV[1] =~ /^set-?replicas$/) {
    esPost("/_flush/synced", "", 1);
    esPut("/${PREFIX}$ARGV[2]/_settings?master_timeout=${ESTIMEOUT}s", "{\"index.number_of_replicas\" : $ARGV[3]}");
    exit 0;
} elsif ($ARGV[1] =~ /^set-?shards-?per-?node$/) {
    esPost("/_flush/synced", "", 1);
    esPut("/${PREFIX}$ARGV[2]/_settings?master_timeout=${ESTIMEOUT}s", "{\"index.routing.allocation.total_shards_per_node\" : $ARGV[3]}");
    exit 0;
} elsif ($ARGV[1] =~ /^set-?allocation-?enable$/) {
    esPost("/_flush/synced", "", 1);
    if ($ARGV[2] eq "null") {
        esPut("/_cluster/settings?master_timeout=${ESTIMEOUT}s", "{ \"persistent\": { \"cluster.routing.allocation.enable\": null}}");
    } else {
        esPut("/_cluster/settings?master_timeout=${ESTIMEOUT}s", "{ \"persistent\": { \"cluster.routing.allocation.enable\": \"$ARGV[2]\"}}");
    }
    exit 0;
} elsif ($ARGV[1] =~ /^allocate-?empty$/) {
    my $result = esPost("/_cluster/reroute?master_timeout=${ESTIMEOUT}s", "{ \"commands\": [{\"allocate_empty_primary\": {\"index\": \"$ARGV[3]\", \"shard\": \"$ARGV[4]\", \"node\": \"$ARGV[2]\", \"accept_data_loss\": true}}]}");
    exit 0;
} elsif ($ARGV[1] =~ /^unflood-?stage$/) {
    esPut("/${PREFIX}$ARGV[2]/_settings?master_timeout=${ESTIMEOUT}s", "{\"index.blocks.read_only_allow_delete\" : null}");
    exit 0;
}

sub dataNodes
{
my ($nodes) = @_;
    my $total = 0;

    foreach my $key (keys %{$nodes}) {
        next if (exists $nodes->{$key}->{attributes} && exists $nodes->{$key}->{attributes}->{data} && $nodes->{$key}->{attributes}->{data} eq "false");
        next if (exists $nodes->{$key}->{settings} && exists $nodes->{$key}->{settings}->{node} && $nodes->{$key}->{settings}->{node}->{data} eq "false");
        $total++;
    }
    return $total;
}


my $health = dbCheckHealth();

my $nodes = esGet("/_nodes");
$main::numberOfNodes = dataNodes($nodes->{nodes});
logmsg "It is STRONGLY recommended that you stop ALL moloch captures and viewers before proceeding.  Use 'db.pl ${main::elasticsearch} backup' to backup db first.\n\n";
if ($main::numberOfNodes == 1) {
    logmsg "There is $main::numberOfNodes elastic search data node, if you expect more please fix first before proceeding.\n\n";
} else {
    logmsg "There are $main::numberOfNodes elastic search data nodes, if you expect more please fix first before proceeding.\n\n";
}

if (int($SHARDS) > $main::numberOfNodes) {
    die "Can't set shards ($SHARDS) greater then the number of nodes ($main::numberOfNodes)";
} elsif ($SHARDS == -1) {
    $SHARDS = $main::numberOfNodes;
    if ($SHARDS > 24) {
        logmsg "Setting # of shards to 24, use --shards for a different number\n";
        $SHARDS = 24;
    }
}

dbVersion(1);

if ($ARGV[1] eq "wipe" && $main::versionNumber != $VERSION) {
    die "Can only use wipe if schema is up to date.  Use upgrade first.";
}

dbCheck();

if ($ARGV[1] =~ /^(init|wipe|clean)/) {

    if ($ARGV[1] eq "init" && $main::versionNumber >= 0) {
        logmsg "It appears this elastic search cluster already has moloch installed (version $main::versionNumber), this will delete ALL data in elastic search! (It does not delete the pcap files on disk.)\n\n";
        waitFor("INIT", "do you want to erase everything?");
    } elsif ($ARGV[1] eq "wipe") {
        logmsg "This will delete ALL session data in elastic search! (It does not delete the pcap files on disk or user info.)\n\n";
        waitFor("WIPE", "do you want to wipe everything?");
    } elsif ($ARGV[1] eq "clean") {
        waitFor("CLEAN", "do you want to clean everything?");
    }
    logmsg "Erasing\n";
    esDelete("/${PREFIX}tags_v3", 1);
    esDelete("/${PREFIX}tags_v2", 1);
    esDelete("/${PREFIX}tags", 1);
    esDelete("/${PREFIX}sequence", 1);
    esDelete("/${PREFIX}sequence_v1", 1);
    esDelete("/${PREFIX}sequence_v2", 1);
    esDelete("/${PREFIX}files_v5", 1);
    esDelete("/${PREFIX}files_v4", 1);
    esDelete("/${PREFIX}files_v3", 1);
    esDelete("/${PREFIX}files", 1);
    esDelete("/${PREFIX}stats", 1);
    esDelete("/${PREFIX}stats_v1", 1);
    esDelete("/${PREFIX}stats_v2", 1);
    esDelete("/${PREFIX}stats_v3", 1);
    esDelete("/${PREFIX}dstats", 1);
    esDelete("/${PREFIX}fields", 1);
    esDelete("/${PREFIX}dstats_v1", 1);
    esDelete("/${PREFIX}dstats_v2", 1);
    esDelete("/${PREFIX}dstats_v3", 1);
    esDelete("/${PREFIX}sessions-*", 1);
    esDelete("/${PREFIX}sessions2-*", 1);
    esDelete("/_template/${PREFIX}template_1", 1);
    esDelete("/_template/${PREFIX}sessions_template", 1);
    esDelete("/_template/${PREFIX}sessions2_template", 1);
    esDelete("/${PREFIX}fields", 1);
    esDelete("/${PREFIX}fields_v1", 1);
    esDelete("/${PREFIX}fields_v2", 1);
    esDelete("/${PREFIX}history_v1-*", 1);
    esDelete("/_template/${PREFIX}history_v1_template", 1);
    esDelete("/${PREFIX}hunts_v1", 1);
    if ($ARGV[1] =~ /^(init|clean)/) {
        esDelete("/${PREFIX}users_v3", 1);
        esDelete("/${PREFIX}users_v4", 1);
        esDelete("/${PREFIX}users_v5", 1);
        esDelete("/${PREFIX}users_v6", 1);
        esDelete("/${PREFIX}users", 1);
        esDelete("/${PREFIX}queries", 1);
        esDelete("/${PREFIX}queries_v1", 1);
        esDelete("/${PREFIX}queries_v2", 1);
    }
    esDelete("/tagger", 1);

    sleep(1);

    exit 0 if ($ARGV[1] =~ "clean");

    logmsg "Creating\n";
    sequenceCreate();
    filesCreate();
    statsCreate();
    dstatsCreate();
    sessions2Update();
    fieldsCreate();
    historyUpdate();
    huntsCreate();
    if ($ARGV[1] =~ "init") {
        usersCreate();
        queriesCreate();
    }
} elsif ($ARGV[1] =~ /^restore$/) {

    logmsg "It is STRONGLY recommended that you stop ALL moloch captures and viewers before proceeding.\n";

    dbCheckForActivity();

    my @indexes = ("users", "sequence", "stats", "queries", "hunts", "files", "fields", "dstats");
    my @filelist = ();
    foreach my $index (@indexes) { # list of data, settings, and mappings files
        push(@filelist, "$ARGV[2].${PREFIX}${index}.json\n") if (-e "$ARGV[2].${PREFIX}${index}.json");
        push(@filelist, "$ARGV[2].${PREFIX}${index}.settings.json\n") if (-e "$ARGV[2].${PREFIX}${index}.settings.json");
        push(@filelist, "$ARGV[2].${PREFIX}${index}.mappings.json\n") if (-e "$ARGV[2].${PREFIX}${index}.mappings.json");
    }
    foreach my $index ("sessions2", "history") { # list of templates
        @filelist = (@filelist, "$ARGV[2].${PREFIX}${index}.template.json\n") if (-e "$ARGV[2].${PREFIX}${index}.template.json");
    }

    push(@filelist, "$ARGV[2].${PREFIX}aliases.json\n") if (-e "$ARGV[2].${PREFIX}aliases.json");

    my @directory = split(/\//,$ARGV[2]);
    my $basename = $directory[scalar(@directory)-1];
    splice(@directory, scalar(@directory)-1, 1);
    my $path = join("/", @directory);

    die "Cannot find files start with ${basename}.${PREFIX} in $path" if (scalar(@filelist) == 0);


    logmsg "\nFollowing files will be used for restore\n\n@filelist\n\n";

    waitFor("RESTORE", "do you want to restore? This will delete ALL data [@indexes] but sessions and history and restore from backups: files start with $basename in $path");

    logmsg "\nStarting Restore...\n\n";

    logmsg "Erasing data ...\n\n";

    esDelete("/${PREFIX}tags_v3", 1);
    esDelete("/${PREFIX}tags_v2", 1);
    esDelete("/${PREFIX}tags", 1);
    esDelete("/${PREFIX}sequence", 1);
    esDelete("/${PREFIX}sequence_v1", 1);
    esDelete("/${PREFIX}sequence_v2", 1);
    esDelete("/${PREFIX}files_v5", 1);
    esDelete("/${PREFIX}files_v4", 1);
    esDelete("/${PREFIX}files_v3", 1);
    esDelete("/${PREFIX}files", 1);
    esDelete("/${PREFIX}stats", 1);
    esDelete("/${PREFIX}stats_v1", 1);
    esDelete("/${PREFIX}stats_v2", 1);
    esDelete("/${PREFIX}stats_v3", 1);
    esDelete("/${PREFIX}dstats", 1);
    esDelete("/${PREFIX}dstats_v1", 1);
    esDelete("/${PREFIX}dstats_v2", 1);
    esDelete("/${PREFIX}dstats_v3", 1);
    esDelete("/${PREFIX}fields", 1);
    esDelete("/${PREFIX}fields_v1", 1);
    esDelete("/${PREFIX}fields_v2", 1);
    esDelete("/${PREFIX}hunts_v1", 1);
    esDelete("/${PREFIX}hunts", 1);
    esDelete("/${PREFIX}users_v3", 1);
    esDelete("/${PREFIX}users_v4", 1);
    esDelete("/${PREFIX}users_v5", 1);
    esDelete("/${PREFIX}users_v6", 1);
    esDelete("/${PREFIX}users", 1);
    esDelete("/${PREFIX}queries", 1);
    esDelete("/${PREFIX}queries_v1", 1);
    esDelete("/${PREFIX}queries_v2", 1);
    esDelete("/_template/${PREFIX}template_1", 1);
    esDelete("/_template/${PREFIX}sessions_template", 1);
    esDelete("/_template/${PREFIX}sessions2_template", 1);
    esDelete("/_template/${PREFIX}history_v1_template", 1);

    logmsg "Importing settings...\n\n";
    foreach my $index (@indexes) { # import settings
        if (-e "$ARGV[2].${PREFIX}${index}.settings.json") {
            open(my $fh, "<", "$ARGV[2].${PREFIX}${index}.settings.json");
            my $data = do { local $/; <$fh> };
            $data = from_json($data);
            my @index = keys %{$data};
            delete $data->{$index[0]}->{settings}->{index}->{creation_date};
            delete $data->{$index[0]}->{settings}->{index}->{provided_name};
            delete $data->{$index[0]}->{settings}->{index}->{uuid};
            delete $data->{$index[0]}->{settings}->{index}->{version};
            my $settings = to_json($data->{$index[0]});
            esPut("/$index[0]", $settings);
            close($fh);
        }
    }

    logmsg "Importing aliases...\n\n";
    if (-e "$ARGV[2].${PREFIX}aliases.json") { # import alias
            open(my $fh, "<", "$ARGV[2].${PREFIX}aliases.json");
            my $data = do { local $/; <$fh> };
            $data = from_json($data);
            foreach my $alias (@{$data}) {
                esAlias("add", $alias->{index}, $alias->{alias}, 1);
            }
    }

    logmsg "Importing mappings...\n\n";
    foreach my $index (@indexes) { # import mappings
        if (-e "$ARGV[2].${PREFIX}${index}.mappings.json") {
            open(my $fh, "<", "$ARGV[2].${PREFIX}${index}.mappings.json");
            my $data = do { local $/; <$fh> };
            $data = from_json($data);
            my @index = keys %{$data};
            my $mappings = $data->{$index[0]}->{mappings};
            my @type = keys %{$mappings};
            esPut("/$index[0]/$type[0]/_mapping?master_timeout=${ESTIMEOUT}s&pretty", to_json($mappings));
            close($fh);
        }
    }

    logmsg "Importing documents...\n\n";
    foreach my $index (@indexes) { # import documents
        if (-e "$ARGV[2].${PREFIX}${index}.json") {
            open(my $fh, "<", "$ARGV[2].${PREFIX}${index}.json");
            my $data = do { local $/; <$fh> };
            esPost("/_bulk", $data);
            close($fh);
        }
    }

    logmsg "Importing templates for Sessions and History...\n\n";
    my @templates = ("sessions2", "history");
    foreach my $template (@templates) { # import templates
        if (-e "$ARGV[2].${PREFIX}${template}.template.json") {
            open(my $fh, "<", "$ARGV[2].${PREFIX}${template}.template.json");
            my $data = do { local $/; <$fh> };
            $data = from_json($data);
            my @template_name = keys %{$data};
            esPut("/_template/$template_name[0]?master_timeout=${ESTIMEOUT}s", to_json($data->{$template_name[0]}));
            close($fh);
        }
    }

    foreach my $template (@templates) { # update mappings
        if (-e "$ARGV[2].${PREFIX}${template}.template.json") {
            open(my $fh, "<", "$ARGV[2].${PREFIX}${template}.template.json");
            my $data = do { local $/; <$fh> };
            $data = from_json($data);
            my @template_name = keys %{$data};
            my $mapping = $data->{$template_name[0]}->{mappings};
            if (($template cmp "sessions2") == 0 && $UPGRADEALLSESSIONS) {
                my $indices = esGet("/${PREFIX}sessions2-*/_alias", 1);
                logmsg "Updating sessions2 mapping for ", scalar(keys %{$indices}), " indices\n" if (scalar(keys %{$indices}) != 0);
                foreach my $i (keys %{$indices}) {
                    progress("$i ");
                    esPut("/$i/session/_mapping?master_timeout=${ESTIMEOUT}s", to_json($mapping), 1);
                }
                logmsg "\n";
            } elsif (($template cmp "history") == 0) {
                my $indices = esGet("/${PREFIX}history_v1-*/_alias", 1);
                logmsg "Updating history mapping for ", scalar(keys %{$indices}), " indices\n" if (scalar(keys %{$indices}) != 0);
                foreach my $i (keys %{$indices}) {
                    progress("$i ");
                    esPut("/$i/history/_mapping?master_timeout=${ESTIMEOUT}s", to_json($mapping), 1);
                }
                logmsg "\n";
            }
            close($fh);
        }
    }
    logmsg "Finished Restore.\n";
} else {

# Remaing is upgrade or upgradenoprompt

# For really old versions don't support upgradenoprompt
    if ($main::versionNumber < 50) {
        logmsg "Can not upgrade directly, please upgrade to Moloch 1.0 or 1.1 first. (Db version $main::VersionNumber)\n\n";
        exit 1;
    }

    if ($health->{status} eq "red") {
        logmsg "Not auto upgrading when elasticsearch status is red.\n\n";
        waitFor("RED", "do you want to really want to upgrade?");
    } elsif ($ARGV[1] ne "upgradenoprompt") {
        logmsg "Trying to upgrade from version $main::versionNumber to version $VERSION.\n\n";
        waitFor("UPGRADE", "do you want to upgrade?");
    }

    logmsg "Starting Upgrade\n";

    esDelete("/${PREFIX}dstats_v2/version/version", 1);
    esDelete("/${PREFIX}dstats_v3/version/version", 1);
    if ($main::versionNumber < 51) {
        dbCheckForActivity();
        esPost("/_flush/synced", "", 1);
        sequenceUpgrade();
        createNewAliasesFromOld("fields", "fields_v2", "fields_v1", \&fieldsCreate);
        createNewAliasesFromOld("queries", "queries_v2", "queries_v1", \&queriesCreate);
        createNewAliasesFromOld("files", "files_v5", "files_v4", \&filesCreate);
        createNewAliasesFromOld("users", "users_v6", "users_v4", \&usersCreate);
        createNewAliasesFromOld("dstats", "dstats_v3", "dstats_v2", \&dstatsCreate);
        createNewAliasesFromOld("stats", "stats_v3", "stats_v2", \&statsCreate);

        historyUpdate();
        sessions2Update();

        esDelete("/${PREFIX}tags_v3", 1);
        esDelete("/${PREFIX}tags_v2", 1);
        esDelete("/${PREFIX}tags", 1);

        huntsCreate();
        checkForOld5Indices();
        setPriority();
    } elsif ($main::versionNumber < 52) {
        historyUpdate();
        fieldsUpdate();
        createNewAliasesFromOld("users", "users_v6", "users_v5", \&usersCreate);
        huntsCreate();
        checkForOld5Indices();
        setPriority();
        queriesUpdate();
        sessions2Update();
    } elsif ($main::versionNumber <= 53) {
        historyUpdate();
        createNewAliasesFromOld("users", "users_v6", "users_v5", \&usersCreate);
        checkForOld5Indices();
        setPriority();
        queriesUpdate();
        sessions2Update();
        fieldsIpDst();
    } elsif ($main::versionNumber <= 58) {
        checkForOld5Indices();
        setPriority();
        usersUpdate();
        huntsUpdate();
        queriesUpdate();
        sessions2Update();
        fieldsIpDst();
    } elsif ($main::versionNumber <= 60) {
        checkForOld5Indices();
        sessions2Update();
        usersUpdate();
    } else {
        logmsg "db.pl is hosed\n";
    }
}

logmsg "Finished\n";

sleep 1;
