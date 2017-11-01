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

use HTTP::Request::Common;
use LWP::UserAgent;
use JSON;
use Data::Dumper;
use POSIX;
use strict;

my $VERSION = 37;
my $verbose = 0;
my $PREFIX = "";
my $NOCHANGES = 0;
my $SHARDS = -1;
my $REPLICAS = -1;
my $HISTORY = 13;
my $NOOPTIMIZE = 0;

################################################################################
sub MIN ($$) { $_[$_[0] > $_[1]] }
sub MAX ($$) { $_[$_[0] < $_[1]] }

sub commify {
    scalar reverse join ',',
    unpack '(A3)*',
    scalar reverse shift
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
    print "\n";
    print "Commands:\n";
    print "  init [<opts>]                - Clear ALL elasticsearch moloch data and create schema\n";
    print "    --shards <shards>          - Number of shards for sessions, default number of nodes\n";
    print "    --replicas <num>           - Number of replicas for sessions, default 0\n";
    print "  upgrade [<opts>]             - Upgrade Moloch's schema in elasticsearch from previous versions\n";
    print "    --shards <shards>          - Number of shards for sessions, default number of nodes\n";
    print "    --replicas <num>           - Number of replicas for sessions, default 0\n";
    print "  wipe                         - Same as init, but leaves user database untouched\n";
    print "  info                         - Information about the database\n";
    print "  users-export <fn>            - Save the users info to <fn>\n";
    print "  users-import <fn>            - Load the users info from <fn>\n";
    print "  optimize                     - Optimize all indices in ES\n";
    print "  mv <old fn> <new fn>         - Move a pcap file in the database (doesn't change disk)\n";
    print "  rm <fn>                      - Remove a pcap file in the database (doesn't change disk)\n";
    print "  rm-missing <node>            - Remove from db any MISSING file on THIS machine for named node\n";
    print "  rm-node <node>               - Remove from db all data for node (doesn't change disk)\n";
    print "  add-missing <node> <dir>     - Add to db any MISSING file on THIS machine for named node and directory\n";
    print "  sync-files  <nodes> <dirs>   - Add/Remove in db any MISSING file on THIS machine for named node(s) and directory(s), both comma separated\n";
    print "  expire <type> <num> [<opts>] - Perform daily ES maintenance and optimize all indices in ES\n";
    print "       type                    - Same as rotateIndex in ini file = hourly,daily,weekly,monthly\n";
    print "       num                     - number of indexes to keep\n";
    print "    --replicas <num>           - Number of replicas for older sessions indices, default 0\n";
    print "    --nooptimize               - Do not optimize session indexes during this operation\n";
    print "    --history <num>            - Number of weeks of history to keep, by default 13\n";
    print "  field disable <exp>          - disable a field from being indexed\n";
    print "  field enable <exp>           - enable a field from being indexed\n";
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
sub esGet
{
    my ($url, $dontcheck) = @_;
    print "GET ${main::elasticsearch}$url\n" if ($verbose > 2);
    my $response = $main::userAgent->get("${main::elasticsearch}$url");
    if (($response->code == 500 && $ARGV[1] ne "init") || ($response->code != 200 && !$dontcheck)) {
      die "Couldn't GET ${main::elasticsearch}$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?";
    }
    my $json = from_json($response->content);
    print "GET RESULT:", Dumper($json), "\n" if ($verbose > 3);
    return $json
}

################################################################################
sub esPost
{
    my ($url, $content, $dontcheck) = @_;

    if ($NOCHANGES && $url !~ /_search/) {
      print "NOCHANGE: PUT ${main::elasticsearch}$url\n";
      return;
    }

    print "POST ${main::elasticsearch}$url\n" if ($verbose > 2);
    print "POST DATA:", Dumper($content), "\n" if ($verbose > 3);
    my $response = $main::userAgent->post("${main::elasticsearch}$url", Content => $content, Content_Type => "application/json");
    if ($response->code == 500 || ($response->code != 200 && $response->code != 201 && !$dontcheck)) {
      print "POST RESULT:", $response->content, "\n" if ($verbose > 3);
      die "Couldn't POST ${main::elasticsearch}$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?";
    }

    my $json = from_json($response->content);
    print "POST RESULT:", Dumper($json), "\n" if ($verbose > 3);
    return $json
}

################################################################################
sub esPut
{
    my ($url, $content, $dontcheck) = @_;

    if ($NOCHANGES) {
      print "NOCHANGE: PUT ${main::elasticsearch}$url\n";
      return;
    }

    print "PUT ${main::elasticsearch}$url\n" if ($verbose > 2);
    print "PUT DATA:", Dumper($content), "\n" if ($verbose > 3);
    my $response = $main::userAgent->request(HTTP::Request::Common::PUT("${main::elasticsearch}$url", Content => $content, Content_Type => "application/json"));
    if ($response->code == 500 || ($response->code != 200 && !$dontcheck)) {
      print Dumper($response);
      die "Couldn't PUT ${main::elasticsearch}$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?\n" . $response->content;
    }

    my $json = from_json($response->content);
    print "PUT RESULT:", Dumper($json), "\n" if ($verbose > 3);
    return $json
}

################################################################################
sub esDelete
{
    my ($url, $dontcheck) = @_;

    if ($NOCHANGES) {
      print "NOCHANGE: DELETE ${main::elasticsearch}$url\n";
      return;
    }

    print "DELETE ${main::elasticsearch}$url\n" if ($verbose > 2);
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

    $main::userAgent->timeout(3600);

    my $status = esGet("/_stats", 1);
    print "Copying " . $status->{indices}->{$PREFIX . $srci}->{primaries}->{docs}->{count} . " elements from ${PREFIX}$srci to ${PREFIX}$dsti\n";

    esPost("/_reindex", to_json({"source" => {"index" => $PREFIX.$srci}, "dest" => {"index" => $PREFIX.$dsti, "version_type" => "external"}, "conflicts" => "proceed"}));

    my $status = esGet("/${PREFIX}${dsti}/_refresh", 1);
    my $status = esGet("/_stats", 1);
    if ($status->{indices}->{$PREFIX . $srci}->{primaries}->{docs}->{count} != $status->{indices}->{$PREFIX . $dsti}->{primaries}->{docs}->{count}) {
        print $status->{indices}->{$PREFIX . $srci}->{primaries}->{docs}->{count}, " != ",  $status->{indices}->{$PREFIX . $dsti}->{primaries}->{docs}->{count}, "\n";
        die "ERROR - Copy failed from $srci to $dsti\n";
    }

    print "\n";
    $main::userAgent->timeout(30);
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
            $url = "/${PREFIX}$index/$type/_search?scroll=10m&size=500";
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
    my ($cmd, $index, $alias) = @_;

    print "Alias cmd $cmd from $index to alias $alias\n" if ($verbose > 0);
    esPost("/_aliases", '{ "actions": [ { "' . $cmd . '": { "index": "' . $PREFIX . $index . '", "alias" : "'. $PREFIX . $alias .'" } } ] }', 1);
}

################################################################################
sub tagsCreate
{
    my $settings = '
{
  "settings": {
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    print "Creating tags_v3 index\n" if ($verbose > 0);
    esPut("/${PREFIX}tags_v3", $settings);
    esAlias("add", "tags_v3", "tags");
    tagsUpdate();
}

################################################################################
sub tagsUpdate
{
    my $mapping = '
{
  "tag": {
    "dynamic": "strict",
    "properties": {
      "n": {
        "type": "integer"
      }
    }
  }
}';

    print "Setting tags_v3 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}tags_v3/tag/_mapping", $mapping);
}
################################################################################
sub sequenceCreate
{
    my $settings = '
{
  "settings": {
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    print "Creating sequence_v1 index\n" if ($verbose > 0);
    esPut("/${PREFIX}sequence_v1", $settings);
    esAlias("add", "sequence_v1", "sequence");
    sequenceUpdate();
}

################################################################################
sub sequenceUpdate
{
    my $mapping = '
{
  "sequence": {
    "_source" : { "enabled": "false" },
    "_all"    : { "enabled": "false" },
    "enabled" : "false"
  }
}';

    print "Setting sequence_v1 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}sequence_v1/sequence/_mapping", $mapping);
}
################################################################################
sub sequenceUpgrade
{
    sequenceCreate();
    my $results = esGet("/${PREFIX}sequence/_search?version=1&size=10000", 0);
    return if ($results->{hits}->{total} == 0);

    foreach my $hit (@{$results->{hits}->{hits}}) {
        esPost("/${PREFIX}sequence_v1/sequence/$hit->{_id}?version_type=external&version=$hit->{_version}", "{}", 0);
    }
    esDelete("/${PREFIX}sequence");
    esAlias("add", "sequence_v1", "sequence");
}
################################################################################
sub filesCreate
{
    my $settings = '
{
  "settings": {
    "number_of_shards": 2,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    print "Creating files_v4 index\n" if ($verbose > 0);
    esPut("/${PREFIX}files_v4", $settings);
    esAlias("add", "files_v4", "files");
    filesUpdate();
}
################################################################################
sub filesUpdate
{
    my $mapping = '
{
  "file": {
    "_all": {"enabled": "false"},
    "_source": {"enabled": "true"},
    "dynamic": "true",
    "dynamic_templates": [
      {
        "any": {
          "match": "*",
          "mapping": {
            "index": "no"
          }
        }
      }
    ],
    "properties": {
      "num": {
        "type": "long"
      },
      "node": {
        "type": "string",
        "index": "not_analyzed"
      },
      "first": {
        "type": "long"
      },
      "name": {
        "type": "string",
        "index": "not_analyzed"
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

    print "Setting files_v4 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}files_v4/file/_mapping", $mapping);
}
################################################################################
sub statsCreate
{
    my $settings = '
{
  "settings": {
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    print "Creating stats index\n" if ($verbose > 0);
    esPut("/${PREFIX}stats_v2", $settings);
    esAlias("add", "stats_v2", "stats");
    statsUpdate();
}

################################################################################
sub statsUpdate
{
my $mapping = '
{
  "stat": {
    "_all": {"enabled": "false"},
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
        "type": "string",
        "index": "not_analyzed"
      },
      "nodeName": {
        "type": "string",
        "index": "not_analyzed"
      },
      "currentTime": {
        "type": "date",
        "format": "epoch_second"
      }
    }
  }
}';

    print "Setting stats mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}stats_v2/stat/_mapping?pretty", $mapping, 1);
}
################################################################################
sub dstatsCreate
{
    my $settings = '
{
  "settings": {
    "number_of_shards": 2,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    print "Creating dstats_v2 index\n" if ($verbose > 0);
    esPut("/${PREFIX}dstats_v2", $settings);
    esAlias("add", "dstats_v2", "dstats");
    dstatsUpdate();
}

################################################################################
sub dstatsUpdate
{
my $mapping = '
{
  "dstat": {
    "_all": {"enabled": "false"},
    "_source": {"enabled": "true"},
    "dynamic": "true",
    "dynamic_templates": [
      {
        "numeric": {
          "match_mapping_type": "long",
          "mapping": {
            "type": "long",
            "index": "no"
          }
        }
      },
      {
        "noindex": {
          "match": "*",
          "mapping": {
            "index": "no"
          }
        }
      }
    ],
    "properties": {
      "nodeName": {
        "type": "string",
        "index": "not_analyzed"
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

    print "Setting dstats_v2 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}dstats_v2/dstat/_mapping?pretty", $mapping, 1);
}
################################################################################
sub fieldsCreate
{
    my $settings = '
{
  "settings": {
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    print "Creating fields index\n" if ($verbose > 0);
    esPut("/${PREFIX}fields_v1", $settings);
    esAlias("add", "fields_v1", "fields");
    fieldsUpdate();
}
################################################################################
sub fieldsUpdate
{
    my $mapping = '
{
  "field": {
    "_all": {"enabled": "false"},
    "_source": {"enabled": "true"},
    "dynamic_templates": [
      {
        "string_template": {
          "match_mapping_type": "string",
          "mapping": {
            "index": "not_analyzed"
          }
        }
      }
    ]
  }
}';

    print "Setting fields mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}fields_v1/field/_mapping", $mapping);

    esPost("/${PREFIX}fields_v1/field/ip", '{
      "friendlyName": "All IP fields",
      "group": "general",
      "help": "Search all ip fields",
      "type": "ip",
      "dbField": "ipall",
      "portField": "portall",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v1/field/port", '{
      "friendlyName": "All port fields",
      "group": "general",
      "help": "Search all port fields",
      "type": "integer",
      "dbField": "portall",
      "regex": "(^port\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.port$)"
    }');
    esPost("/${PREFIX}fields_v1/field/rir", '{
      "friendlyName": "All rir fields",
      "group": "general",
      "help": "Search all rir fields",
      "type": "uptermfield",
      "dbField": "all",
      "regex": "(^rir\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.rir$)"
    }');
    esPost("/${PREFIX}fields_v1/field/country", '{
      "friendlyName": "All country fields",
      "group": "general",
      "help": "Search all country fields",
      "type": "uptermfield",
      "dbField": "all",
      "regex": "(^country\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.country$)"
    }');
    esPost("/${PREFIX}fields_v1/field/asn", '{
      "friendlyName": "All ASN fields",
      "group": "general",
      "help": "Search all ASN fields",
      "type": "textfield",
      "dbField": "all",
      "regex": "(^asn\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.asn$)"
    }');
    esPost("/${PREFIX}fields_v1/field/host", '{
      "friendlyName": "All Host fields",
      "group": "general",
      "help": "Search all Host fields",
      "type": "lotextfield",
      "dbField": "all",
      "regex": "(^host\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.host$)"
    }');
    esPost("/${PREFIX}fields_v1/field/ip.src", '{
      "friendlyName": "Src IP",
      "group": "general",
      "help": "Source IP",
      "type": "ip",
      "dbField": "a1",
      "portField": "p1",
      "category": "ip"
    }');
    esPost("/${PREFIX}fields_v1/field/port.src", '{
      "friendlyName": "Src Port",
      "group": "general",
      "help": "Source Port",
      "type": "integer",
      "dbField": "p1",
      "category": "port"
    }');
    esPost("/${PREFIX}fields_v1/field/asn.src", '{
      "friendlyName": "Src ASN",
      "group": "general",
      "help": "GeoIP ASN string calculated from the source IP",
      "type": "textfield",
      "dbField": "as1",
      "rawField": "rawas1",
      "category": "asn"
    }');
    esPost("/${PREFIX}fields_v1/field/country.src", '{
      "friendlyName": "Src Country",
      "group": "general",
      "help": "Source Country",
      "type": "uptermfield",
      "dbField": "g1",
      "category": "country"
    }');
    esPost("/${PREFIX}fields_v1/field/rir.src", '{
      "friendlyName": "Src RIR",
      "group": "general",
      "help": "Source RIR",
      "type": "uptermfield",
      "dbField": "rir1",
      "category": "rir"
    }');
    esPost("/${PREFIX}fields_v1/field/ip.dst", '{
      "friendlyName": "Dst IP",
      "group": "general",
      "help": "Destination IP",
      "type": "ip",
      "dbField": "a2",
      "portField": "p2",
      "category": "ip"
    }');
    esPost("/${PREFIX}fields_v1/field/port.dst", '{
      "friendlyName": "Dst Port",
      "group": "general",
      "help": "Source Port",
      "type": "integer",
      "dbField": "p2",
      "category": "port"
    }');
    esPost("/${PREFIX}fields_v1/field/asn.dst", '{
      "friendlyName": "Dst ASN",
      "group": "general",
      "help": "GeoIP ASN string calculated from the destination IP",
      "type": "textfield",
      "dbField": "as2",
      "rawField": "rawas2",
      "category": "asn"
    }');
    esPost("/${PREFIX}fields_v1/field/country.dst", '{
      "friendlyName": "Dst Country",
      "group": "general",
      "help": "Destination Country",
      "type": "uptermfield",
      "dbField": "g2",
      "category": "country"
    }');
    esPost("/${PREFIX}fields_v1/field/rir.dst", '{
      "friendlyName": "Dst RIR",
      "group": "general",
      "help": "Destination RIR",
      "type": "uptermfield",
      "dbField": "rir2",
      "category": "rir"
    }');
    esPost("/${PREFIX}fields_v1/field/bytes", '{
      "friendlyName": "Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent AND received in a session",
      "type": "integer",
      "dbField": "by"
    }');
    esPost("/${PREFIX}fields_v1/field/bytes.src", '{
      "friendlyName": "Src Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent by source in a session",
      "type": "integer",
      "dbField": "by1"
    }');
    esPost("/${PREFIX}fields_v1/field/bytes.dst", '{
      "friendlyName": "Dst Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent by destination in a session",
      "type": "integer",
      "dbField": "by2"
    }');
    esPost("/${PREFIX}fields_v1/field/databytes", '{
      "friendlyName": "Data bytes",
      "group": "general",
      "help": "Total number of data bytes sent AND received in a session",
      "type": "integer",
      "dbField": "db"
    }');
    esPost("/${PREFIX}fields_v1/field/databytes.src", '{
      "friendlyName": "Src data bytes",
      "group": "general",
      "help": "Total number of data bytes sent by source in a session",
      "type": "integer",
      "dbField": "db1"
    }');
    esPost("/${PREFIX}fields_v1/field/databytes.dst", '{
      "friendlyName": "Dst data bytes",
      "group": "general",
      "help": "Total number of data bytes sent by destination in a session",
      "type": "integer",
      "dbField": "db2"
    }');
    esPost("/${PREFIX}fields_v1/field/packets", '{
      "friendlyName": "Packets",
      "group": "general",
      "help": "Total number of packets sent AND received in a session",
      "type": "integer",
      "dbField": "pa"
    }');
    esPost("/${PREFIX}fields_v1/field/packets.src", '{
      "friendlyName": "Src Packets",
      "group": "general",
      "help": "Total number of packets sent by source in a session",
      "type": "integer",
      "dbField": "pa1"
    }');
    esPost("/${PREFIX}fields_v1/field/packets.dst", '{
      "friendlyName": "Dst Packets",
      "group": "general",
      "help": "Total number of packets sent by destination in a session",
      "type": "integer",
      "dbField": "pa2"
    }');
    esPost("/${PREFIX}fields_v1/field/ip.protocol", '{
      "friendlyName": "IP Protocol",
      "group": "general",
      "help": "IP protocol number or friendly name",
      "type": "lotermfield",
      "dbField": "pr",
      "transform": "ipProtocolLookup"
    }');
    esPost("/${PREFIX}fields_v1/field/id", '{
      "friendlyName": "Moloch ID",
      "group": "general",
      "help": "Moloch ID for the session",
      "type": "termfield",
      "dbField": "_id",
      "noFacet": "true"

    }');
    esPost("/${PREFIX}fields_v1/field/rootId", '{
      "friendlyName": "Moloch Root ID",
      "group": "general",
      "help": "Moloch ID of the first session in a multi session stream",
      "type": "termfield",
      "dbField": "ro"
    }');
    esPost("/${PREFIX}fields_v1/field/node", '{
      "friendlyName": "Moloch Node",
      "group": "general",
      "help": "Moloch node name the session was recorded on",
      "type": "termfield",
      "dbField": "no"
    }');
    esPost("/${PREFIX}fields_v1/field/file", '{
      "friendlyName": "Filename",
      "group": "general",
      "help": "Moloch offline pcap filename",
      "type": "fileand",
      "dbField": "fileand"
    }');
    esPost("/${PREFIX}fields_v1/field/payload8.src.hex", '{
      "friendlyName": "Payload Src Hex",
      "group": "general",
      "help": "First 8 bytes of source payload in hex",
      "type": "lotermfield",
      "dbField": "fb1",
      "aliases": ["payload.src"]
    }');
    esPost("/${PREFIX}fields_v1/field/payload8.src.utf8", '{
      "friendlyName": "Payload Src UTF8",
      "group": "general",
      "help": "First 8 bytes of source payload in utf8",
      "type": "termfield",
      "dbField": "fb1",
      "transform": "utf8ToHex",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v1/field/payload8.dst.hex", '{
      "friendlyName": "Payload Dst Hex",
      "group": "general",
      "help": "First 8 bytes of destination payload in hex",
      "type": "lotermfield",
      "dbField": "fb2",
      "aliases": ["payload.dst"]
    }');
    esPost("/${PREFIX}fields_v1/field/payload8.dst.utf8", '{
      "friendlyName": "Payload Dst UTF8",
      "group": "general",
      "help": "First 8 bytes of destination payload in utf8",
      "type": "termfield",
      "dbField": "fb2",
      "transform": "utf8ToHex",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v1/field/payload8.hex", '{
      "friendlyName": "Payload Hex",
      "group": "general",
      "help": "First 8 bytes of payload in hex",
      "type": "lotermfield",
      "dbField": "fballhex",
      "regex": "^payload8.(src|dst).hex$"
    }');
    esPost("/${PREFIX}fields_v1/field/payload8.utf8", '{
      "friendlyName": "Payload UTF8",
      "group": "general",
      "help": "First 8 bytes of payload in hex",
      "type": "lotermfield",
      "dbField": "fballutf8",
      "regex": "^payload8.(src|dst).utf8$"
    }');
    esPost("/${PREFIX}fields_v1/field/scrubbed.by", '{
      "friendlyName": "Scrubbed By",
      "group": "general",
      "help": "SPI data was scrubbed by",
      "type": "lotermfield",
      "dbField": "scrubby"
    }');
    esPost("/${PREFIX}fields_v1/field/view", '{
      "friendlyName": "View Name",
      "group": "general",
      "help": "Moloch view name",
      "type": "viewand",
      "dbField": "viewand",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v1/field/starttime", '{
      "friendlyName": "Start Time",
      "group": "general",
      "help": "Session Start Time",
      "type": "seconds",
      "dbField": "fp"
    }');
    esPost("/${PREFIX}fields_v1/field/stoptime", '{
      "friendlyName": "Stop Time",
      "group": "general",
      "help": "Session Stop Time",
      "type": "seconds",
      "dbField": "lp"
    }');
}

################################################################################
sub queriesCreate
{
    my $settings = '
{
  "settings": {
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    print "Creating queries index\n" if ($verbose > 0);
    esPut("/${PREFIX}queries_v1", $settings);
    queriesUpdate();
}
################################################################################
sub queriesUpdate
{
    my $mapping = '
{
  "query": {
    "_all": {"enabled": "false"},
    "_source": {"enabled": "true"},
    "dynamic": "strict",
    "properties": {
      "name": {
        "type": "string",
        "index": "not_analyzed"
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
        "type": "string",
        "index": "not_analyzed"
      },
      "action": {
        "type": "string",
        "index": "not_analyzed"
      },
      "creator": {
        "type": "string",
        "index": "not_analyzed"
      },
      "tags": {
        "type": "string",
        "index": "not_analyzed"
      }
    }
  }
}';

    print "Setting queries mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}queries_v1/query/_mapping?pretty", $mapping);
    esAlias("add", "queries_v1", "queries");
}

################################################################################
sub sessionsUpdate
{
    my $mapping = '
{
  "session": {
    "_all": {"enabled": "false"},
    "dynamic": "true",
    "dynamic_templates": [
      {
        "template_hdrs": {
          "path_match": "hdrs.*",
          "match_mapping_type": "string",
          "mapping": {
            "type": "string",
            "index": "no",
            "fields": {
              "snow": {"type": "string", "analyzer" : "snowball"},
              "raw": {"type": "string", "index" : "not_analyzed"}
            }
          }
        }
      }, {
        "template_georir": {
          "match_pattern": "regex",
          "path_match": ".*-(geo|rir|term)$",
          "match_mapping_type": "string",
          "mapping": {
            "type": "string",
            "index": "not_analyzed"
          }
        }
      }, {
        "template_string": {
          "match_mapping_type": "string",
          "mapping": {
            "type": "string",
            "index": "no",
            "fields": {
              "snow" : {"type": "string", "analyzer" : "snowball"},
              "raw" : {"type": "string", "index" : "not_analyzed"}
            }
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
      "ipSrc": {
        "type": "ip"
      },
      "portSrc": {
        "type": "integer"
      },
      "ipDst": {
        "type": "ip"
      },
      "portDst": {
        "type": "integer"
      },
      "us": {
        "type": "string",
        "analyzer": "url_analyzer",
        "copy_to": "rawus",
        "norms": {"enabled": "false"}
      },
      "rawus": {
        "type": "string",
        "index": "not_analyzed"
      },
      "uscnt": {
        "type": "integer"
      },
      "ua": {
        "type": "string",
        "analyzer": "snowball",
        "copy_to": "rawua",
        "norms": {"enabled": "false"}
      },
      "rawua": {
        "type": "string",
        "index": "not_analyzed"
      },
      "uacnt": {
        "type": "integer"
      },
      "ps": {
        "type": "long",
        "index": "no"
      },
      "psl": {
        "type": "integer",
        "index": "no"
      },
      "fs": {
        "type": "long"
      },
      "lp": {
        "type": "long",
        "doc_values": "true"
      },
      "lpd": {
        "type": "date",
        "doc_values": "true"
      },
      "fp": {
        "type": "long",
        "doc_values": "true"
      },
      "fpd": {
        "type": "date",
        "doc_values": "true"
      },
      "a1": {
        "type": "long",
        "doc_values": "true"
      },
      "g1": {
        "type": "string",
        "index": "not_analyzed"
      },
      "as1": {
        "type": "string",
        "analyzer": "snowball",
        "copy_to": "rawas1",
        "norms": {"enabled": "false"}
      },
      "rawas1": {
        "type": "string",
        "index": "not_analyzed"
      },
      "rir1": {
        "type": "string",
        "index": "not_analyzed"
      },
      "p1": {
        "type": "integer",
        "doc_values": "true"
      },
      "fb1": {
        "type": "string",
        "index": "not_analyzed"
      },
      "a2": {
        "type": "long",
        "doc_values": "true"
      },
      "g2": {
        "type": "string",
        "index": "not_analyzed"
      },
      "as2": {
        "type": "string",
        "analyzer": "snowball",
        "copy_to": "rawas2",
        "norms": {"enabled": "false"}
      },
      "rawas2": {
        "type": "string",
        "index": "not_analyzed"
      },
      "rir2": {
        "type": "string",
        "index": "not_analyzed"
      },
      "p2": {
        "type": "integer",
        "doc_values": "true"
      },
      "fb2": {
        "type": "string",
        "index": "not_analyzed"
      },
      "xff": {
        "type": "long"
      },
      "xffcnt": {
        "type": "integer"
      },
      "xffscnt": {
        "type": "integer"
      },
      "gxff": {
        "type": "string",
        "index": "not_analyzed"
      },
      "asxff": {
        "type": "string",
        "analyzer": "snowball",
        "copy_to": "rawasxff",
        "norms": {"enabled": "false"}
      },
      "rawasxff": {
        "type": "string",
        "index": "not_analyzed"
      },
      "rirxff": {
        "type": "string",
        "index": "not_analyzed"
      },
      "hmd5cnt": {
        "type": "short"
      },
      "hmd5": {
        "type": "string",
        "index": "not_analyzed"
      },
      "dnshocnt": {
        "type": "integer"
      },
      "dnsho": {
        "type": "string",
        "index": "not_analyzed"
      },
      "dnsip": {
        "type": "long"
      },
      "dnsipcnt": {
        "type": "integer"
      },
      "gdnsip": {
        "type": "string",
        "index": "not_analyzed"
      },
      "asdnsip": {
        "type": "string",
        "analyzer": "snowball",
        "copy_to": "rawasdnsip",
        "norms": {"enabled": "false"}
      },
      "rawasdnsip": {
        "type": "string",
        "index": "not_analyzed"
      },
      "rirdnsip": {
        "type": "string",
        "index": "not_analyzed"
      },
      "pr": {
        "type": "short"
      },
      "pa": {
        "type": "integer"
      },
      "pa1": {
        "type": "integer"
      },
      "pa2": {
        "type": "integer"
      },
      "by": {
        "type": "long"
      },
      "by1": {
        "type": "long"
      },
      "by2": {
        "type": "long"
      },
      "db": {
        "type": "long"
      },
      "db1": {
        "type": "long"
      },
      "db2": {
        "type": "long"
      },
      "ro": {
        "type": "string",
        "index": "not_analyzed"
      },
      "no": {
        "type": "string",
        "index": "not_analyzed"
      },
      "ho": {
        "type": "string",
        "index": "not_analyzed"
      },
      "hocnt": {
        "type": "integer"
      },
      "ta": {
        "type": "integer"
      },
      "tacnt": {
        "type": "integer"
      },
      "hh": {
        "type": "integer"
      },
      "hh1": {
        "type": "integer"
      },
      "hh2": {
        "type": "integer"
      },
      "hh1cnt": {
        "type": "integer"
      },
      "hh2cnt": {
        "type": "integer"
      },
      "hsver": {
        "type": "string",
        "index": "not_analyzed"
      },
      "hsvercnt": {
        "type": "integer"
      },
      "hdver": {
        "type": "string",
        "index": "not_analyzed"
      },
      "hdvercnt": {
        "type": "integer"
      },
      "hpath": {
        "type": "string",
        "index": "not_analyzed"
      },
      "hpathcnt": {
        "type": "integer"
      },
      "hkey": {
        "type": "string",
        "index": "not_analyzed"
      },
      "hkeycnt": {
        "type": "integer"
      },
      "hval": {
        "type": "string",
        "index": "not_analyzed"
      },
      "hvalcnt": {
        "type": "integer"
      },
      "user": {
        "type": "string",
        "index": "not_analyzed"
      },
      "usercnt": {
        "type": "integer"
      },
      "tls": {
        "type": "object",
        "dynamic": "strict",
        "properties": {
          "iCn": {
            "type": "string",
            "index": "not_analyzed"
          },
          "iOn": {
            "type": "string",
            "analyzer": "snowball",
            "norms": {"enabled": "false"},
            "fields": {
              "rawiOn": {"type": "string", "index": "not_analyzed"}
            }
          },
          "sCn": {
            "type": "string",
            "index": "not_analyzed"
          },
          "sOn": {
            "type": "string",
            "analyzer": "snowball",
            "norms": {"enabled": "false"},
            "fields": {
              "rawsOn": {"type": "string", "index": "not_analyzed"}
            }
          },
          "sn": {
            "type": "string",
            "index": "not_analyzed"
          },
          "alt": {
            "type": "string",
            "index": "not_analyzed"
          },
          "altcnt": {
            "type": "integer"
          },
          "notBefore": {
            "type": "long",
            "index": "not_analyzed"
          },
          "notAfter": {
            "type": "long",
            "index": "not_analyzed"
          },
          "diffDays": {
            "type": "integer",
            "index": "not_analyzed"
          },
          "hash": {
            "type": "string",
            "index": "not_analyzed"
          }
        }
      },
      "tlscnt": {
        "type": "integer"
      },
      "sshkey": {
        "type": "string",
        "index": "not_analyzed"
      },
      "sshkeycnt": {
        "type": "short"
      },
      "sshver": {
        "type": "string",
        "index": "not_analyzed"
      },
      "sshvercnt": {
        "type": "short"
      },
      "euacnt": {
        "type": "short"
      },
      "eua": {
        "type": "string",
        "analyzer": "snowball",
        "copy_to": "raweua",
        "norms": {"enabled": "false"}
      },
      "raweua": {
        "type": "string",
        "index": "not_analyzed"
      },
      "esubcnt": {
        "type": "short"
      },
      "esub": {
        "type": "string",
        "analyzer": "snowball",
        "copy_to": "rawesub",
        "norms": {"enabled": "false"}
      },
      "rawesub": {
        "type": "string",
        "index": "not_analyzed"
      },
      "eidcnt": {
        "type": "short"
      },
      "eid": {
        "type": "string",
        "index": "not_analyzed"
      },
      "ectcnt": {
        "type": "short"
      },
      "ect": {
        "type": "string",
        "index": "not_analyzed"
      },
      "emvcnt": {
        "type": "short"
      },
      "emv": {
        "type": "string",
        "index": "not_analyzed"
      },
      "efncnt": {
        "type": "short"
      },
      "efn": {
        "type": "string",
        "index": "not_analyzed"
      },
      "emd5cnt": {
        "type": "short"
      },
      "emd5": {
        "type": "string",
        "index": "not_analyzed"
      },
      "esrccnt": {
        "type": "short"
      },
      "esrc": {
        "type": "string",
        "index": "not_analyzed"
      },
      "edstcnt": {
        "type": "short"
      },
      "edst": {
        "type": "string",
        "index": "not_analyzed"
      },
      "eho": {
        "type": "string",
        "index": "not_analyzed"
      },
      "ehocnt": {
        "type": "integer"
      },
      "eip": {
        "type": "long"
      },
      "eipcnt": {
        "type": "integer"
      },
      "ehh": {
        "type": "string",
        "index": "not_analyzed"
      },
      "ehhcnt": {
        "type": "integer"
      },
      "geip": {
        "type": "string",
        "index": "not_analyzed"
      },
      "aseip": {
        "type": "string",
        "analyzer": "snowball",
        "copy_to": "rawaseip",
        "norms": {"enabled": "false"}
      },
      "rawaseip": {
        "type": "string",
        "index": "not_analyzed"
      },
      "rireip": {
        "type": "string",
        "index": "not_analyzed"
      },
      "ircnck": {
        "type": "string",
        "index": "not_analyzed"
      },
      "ircnckcnt": {
        "type": "integer"
      },
      "ircch": {
        "type": "string",
        "index": "not_analyzed"
      },
      "ircchcnt": {
        "type": "integer"
      },
      "hdrs": {
        "type": "object",
        "dynamic": "true"
      },
      "plugin": {
        "type": "object",
        "dynamic": "true"
      },
      "scrubat": {
        "type": "date"
      },
      "scrubby": {
        "type": "string",
        "index": "not_analyzed"
      },
      "smbdmcnt": {
        "type": "short"
      },
      "smbdm": {
        "type": "string",
        "index": "not_analyzed"
      },
      "smbfncnt": {
        "type": "short"
      },
      "smbfn": {
        "type": "string",
        "index": "not_analyzed"
      },
      "smbhocnt": {
        "type": "short"
      },
      "smbho": {
        "type": "string",
        "index": "not_analyzed"
      },
      "smboscnt": {
        "type": "short"
      },
      "smbos": {
        "type": "string",
        "index": "not_analyzed"
      },
      "smbshcnt": {
        "type": "short"
      },
      "smbsh": {
        "type": "string",
        "index": "not_analyzed"
      },
      "smbusercnt": {
        "type": "short"
      },
      "smbuser": {
        "type": "string",
        "index": "not_analyzed"
      },
      "smbvercnt": {
        "type": "short"
      },
      "smbver": {
        "type": "string",
        "index": "not_analyzed"
      },
      "socksip": {
        "type": "long"
      },
      "gsocksip": {
        "type": "string",
        "index": "not_analyzed"
      },
      "assocksip": {
        "type": "string",
        "analyzer": "snowball",
        "copy_to": "rawassocksip",
        "norms": {"enabled": "false"}
      },
      "rawassocksip": {
        "type": "string",
        "index": "not_analyzed"
      },
      "rirsocksip": {
        "type": "string",
        "index": "not_analyzed"
      },
      "sockspo": {
        "type": "integer"
      },
      "socksuser": {
        "type": "string",
        "index": "not_analyzed"
      },
      "socksho": {
        "type": "string",
        "index": "not_analyzed"
      }
    }
  }
}
';

$REPLICAS = 0 if ($REPLICAS < 0);
my $shardsPerNode = ceil($SHARDS * ($REPLICAS+1) / $main::numberOfNodes);

    my $template = '
{
  "template": "' . $PREFIX . 'session*",
  "settings": {
    "index": {
      "routing.allocation.total_shards_per_node": ' . $shardsPerNode . ',
      "refresh_interval": "60s",
      "number_of_shards": ' . $SHARDS . ',
      "number_of_replicas": ' . $REPLICAS . ',
      "analysis": {
        "analyzer": {
          "url_analyzer": {
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

    print "Creating sessions template\n" if ($verbose > 0);
    #print "$template\n";
    esPut("/_template/${PREFIX}sessions_template", $template);

    my $indices = esGet("/${PREFIX}sessions-*/_aliases", 1);

    print "Updating sessions mapping for ", scalar(keys %{$indices}), " indices\n" if (scalar(keys %{$indices}) != 0);
    foreach my $i (keys %{$indices}) {
        progress("$i ");
        esPut("/$i/session/_mapping", $mapping, 1);

        # Before version 12 had soft, change to node, requires a close and open
        if ($main::versionNumber < 12) {
            esPost("/$i/_close", "");
            #esPut("/$i/_settings", '{"index.fielddata.cache": "node", "index.cache.field.type" : "node", "index.store.type": "mmapfs"}');
            esPut("/$i/_settings", '{"index.fielddata.cache": "node", "index.cache.field.type" : "node"}');
            esPost("/$i/_open", "");
        }
    }

    print "\n";
}

################################################################################
sub historyUpdate
{
    my $mapping = '
{
  "history": {
    "_all": {"enabled": "false"},
    "_source": {"enabled": "true"},
    "dynamic": "strict",
    "properties": {
      "uiPage": {
        "type": "string",
        "index": "not_analyzed"
      },
      "userId": {
        "type": "string",
        "index": "not_analyzed"
      },
      "method": {
        "type": "string",
        "index": "not_analyzed"
      },
      "api": {
        "type": "string",
        "index": "not_analyzed"
      },
      "expression": {
        "type": "string",
        "index": "not_analyzed"
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
        "type": "string",
        "index": "not_analyzed"
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
      }
    }
  }
}';

 my $template = '
{
  "template": "' . $PREFIX . 'history_v1-*",
  "settings": {
      "number_of_shards": 2,
      "number_of_replicas": 0,
      "auto_expand_replicas": "0-1"
    },
  "mappings":' . $mapping . '
}';

print "Creating history template\n" if ($verbose > 0);
esPut("/_template/${PREFIX}history_v1_template", $template);

my $indices = esGet("/${PREFIX}history_v1-*/_aliases", 1);

print "Updating history mapping for ", scalar(keys %{$indices}), " indices\n" if (scalar(keys %{$indices}) != 0);
foreach my $i (keys %{$indices}) {
    progress("$i ");
    esPut("/$i/history/_mapping", $mapping, 1);
}

print "\n";
}
################################################################################

################################################################################
sub usersCreate
{
    my $settings = '
{
  "settings": {
    "number_of_shards": 1,
    "number_of_replicas": 0,
    "auto_expand_replicas": "0-3"
  }
}';

    print "Creating users_v4 index\n" if ($verbose > 0);
    esPut("/${PREFIX}users_v4", $settings);
    esAlias("add", "users_v4", "users");
    usersUpdate();
}
################################################################################
sub usersUpdate
{
    my $mapping = '
{
  "user": {
    "_all": {"enabled": "false"},
    "_source": {"enabled": "true"},
    "dynamic": "strict",
    "properties": {
      "userId": {
        "type": "string",
        "index": "not_analyzed"
      },
      "userName": {
        "type": "string",
        "index": "not_analyzed"
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
      "passStore": {
        "type": "string",
        "index": "not_analyzed"
      },
      "expression": {
        "type": "string",
        "index": "not_analyzed"
      },
      "settings": {
        "type": "object",
        "dynamic": "true"
      },
      "views": {
        "type": "object",
        "dynamic": "true"
      },
      "columnConfigs": {
        "type": "object",
        "dynamic": "true"
      },
      "spiviewFieldConfigs": {
        "type": "object",
        "dynamic": "true"
      },
      "tableStates": {
        "type": "object",
        "dynamic": "true"
      }
    }
  }
}';

    print "Setting users_v4 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}users_v4/user/_mapping?pretty", $mapping);
}
################################################################################
sub createAliasedFromNonAliased
{
    my ($name, $newName, $createFunction) = @_;

    my $indices = esGet("/${PREFIX}{$newName},${PREFIX}${name}/_aliases?ignore_unavailable=1", 1);

    # Need to create new name
    if (!exists $indices->{"${PREFIX}{$newName}"}) {
        $createFunction->();
        sleep 1;
        $indices = esGet("/${PREFIX}${newName},${PREFIX}${name}/_aliases?ignore_unavailable=1", 1);
    }

    # Copy old index to new index
    if (exists $indices->{"${PREFIX}${newName}"} && exists $indices->{"${PREFIX}${name}"}) {
        esCopy("${name}", "${newName}");
    }

    # Delete old index and add alias.  Do in a loop since there is a race condition of capture
    # processes trying to save their information.
    $indices = esGet("/${PREFIX}${newName},${PREFIX}${name}/_aliases?ignore_unavailable=1", 1);
    while (exists $indices->{"${PREFIX}${name}"} || ! exists $indices->{"${PREFIX}${newName}"}->{aliases}->{"${PREFIX}${name}"}) {
        esDelete("/${PREFIX}${name}", 1);
        esAlias("add", "${newName}", "${name}");
        $indices = esGet("/${PREFIX}${newName},${PREFIX}${name}/_aliases?ignore_unavailable=1", 1);
    }
}
################################################################################
sub createNewAliasesFromOld
{
    my ($alias, $newName, $oldName, $createFunction) = @_;
    $createFunction->();
    esAlias("remove", $oldName, $alias);
    esCopy($oldName, $newName);
    esDelete("/${PREFIX}${oldName}", 1);
}
################################################################################
sub time2index
{
my($type, $prefix, $t) = @_;

    my @t = gmtime($t);
    if ($type eq "hourly") {
        return sprintf("${PREFIX}${prefix}%02d%02d%02dh%02d", $t[5] % 100, $t[4]+1, $t[3], $t[2]);
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
    if ($main::esVersion >= 50000) {
        $main::OPTIMIZE = "_forcemerge";
    } else {
        $main::OPTIMIZE = "_optimize";
    }
    return $esversion;
}
################################################################################
sub dbVersion {
my ($loud) = @_;

    my $version = esGet("/${PREFIX}dstats/version/version", 1);

    my $found = $version->{found};

    if (!defined $found) {
        print "This is a fresh Moloch install\n" if ($loud);
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
    print "This upgrade requires all capture nodes to be stopped.  Checking\n";
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
        print("WARNING elasticsearch health is '$health->{status}' instead of 'green', things may be broken\n\n");
    }
    return $health;
}
################################################################################
sub dbCheck {
    my $esversion = dbESVersion();
    my @parts = split(/\./, $esversion->{version}->{number});
    $main::esVersion = int($parts[0]*100*100) + int($parts[1]*100) + int($parts[2]);

    if ($main::esVersion < 20400 ||
        ($main::esVersion >= 50000 && $main::esVersion < 50102) ||
        ($main::esVersion == 50300) ||
        ($main::esVersion >= 60000)
    ) {
        print("Currently using Elasticsearch version ", $esversion->{version}->{number}, " which isn't supported\n",
              "* 5.6.x is recommended\n",
              "* 2.4.x is supported\n",
              "* 5.0 - 5.1.1, 5.3.0 are not supported\n",
              "* 6.x is not supported\n",
              "\n",
              "Instructions: https://github.com/aol/moloch/wiki/FAQ#How_do_I_upgrade_elasticsearch\n",
              "Make sure to restart any viewer or capture after upgrading!\n"
             );
        exit (1);
    }

    my $error = 0;
    my $nodes = esGet("/_nodes?flat_settings");
    my $nodeStats = esGet("/_nodes/stats");

    if ($main::esVersion < 50000) {
        esPut("/_cluster/settings", '{"persistent": {"threadpool.search.queue_size":10000}}');
    } else {
        # ALW - Not supported with 5.0, might need to require user setting
        # esPut("/_cluster/settings", '{"persistent": {"thread_pool.search.queue_size":10000}}');
    }

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
            print ("\nERROR: On node ", $node->{name}, " machine ", ($node->{hostname} || $node->{host}), " in file ", $node->{settings}->{config}, "\n");
            print($errstr);
        }

        if ($warnstr) {
            print ("\nWARNING: On node ", $node->{name}, " machine ", ($node->{hostname} || $node->{host}), " in file ", $node->{settings}->{config}, "\n");
            print($warnstr);
        }
    }

    if ($error) {
        print "\nFix above errors before proceeding\n";
        exit (1);
    }
}
################################################################################
sub checkForOldIndices {
    my $result = esGet("/_all/_settings/index.version.created?pretty");
    my $found = 0;

    while ( my ($key, $value) = each (%{$result})) {
        if ($value->{settings}->{index}->{version}->{created} < 2000000) {
            print "WARNING: You must delete index '$key' before upgrading to ES 5\n";
            $found = 1;
        }
    }

    if ($found) {
        print "\nYou MUST delete (and optionally re-add) the indices above while still on ES 2.x or ES 5.x will NOT start.\n\n";
    }
}
################################################################################
sub progress {
    my ($msg) = @_;
    if ($verbose == 1) {
        local $| = 1;
        print ".";
    } elsif ($verbose == 2) {
        local $| = 1;
        print "$msg";
    }
}
################################################################################
sub optimizeOther {
    print "Optimizing Admin Indices\n";
    foreach my $i ("${PREFIX}stats_v2", "${PREFIX}dstats_v2", "${PREFIX}files_v4", "${PREFIX}sequence_v1", "${PREFIX}tags_v3", "${PREFIX}users_v4") {
        progress("$i ");
        esGet("/$i/$main::OPTIMIZE?max_num_segments=1", 1);
        esPost("/$i/_upgrade", "", 1);
    }
    print "\n";
    print "\n" if ($verbose > 0);
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
        } elsif ($ARGV[$pos] eq "--nooptimize") {
	    $NOOPTIMIZE = 1;
        } else {
            print "Unknown option '", $ARGV[$pos], "'\n";
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
    } else {
        showHelp("Unknkown global option $ARGV[0]")
    }
    shift @ARGV;
}

showHelp("Help:") if ($ARGV[1] =~ /^help$/);
showHelp("Missing arguments") if (@ARGV < 2);
showHelp("Unknown command '$ARGV[1]'") if ($ARGV[1] !~ /^(init|initnoprompt|info|wipe|upgrade|upgradenoprompt|users-?import|users-?export|expire|rotate|optimize|mv|rm|rm-?missing|rm-?node|add-?missing|field|force-?put-?version|sync-?files)$/);
showHelp("Missing arguments") if (@ARGV < 3 && $ARGV[1] =~ /^(users-?import|users-?export|rm|rm-?missing|rm-?node)$/);
showHelp("Missing arguments") if (@ARGV < 4 && $ARGV[1] =~ /^(field|add-?missing|sync-files)$/);
showHelp("Must have both <old fn> and <new fn>") if (@ARGV < 4 && $ARGV[1] =~ /^(mv)$/);
showHelp("Must have both <type> and <num> arguments") if (@ARGV < 4 && $ARGV[1] =~ /^(rotate|expire)$/);

parseArgs(2) if ($ARGV[1] =~ /^(init|initnoprompt|upgrade|upgradenoprompt)$/);

$main::userAgent = LWP::UserAgent->new(timeout => 30);

if ($ARGV[0] =~ /^http/) {
    $main::elasticsearch = $ARGV[0];
} else {
    $main::elasticsearch = "http://$ARGV[0]";
}

if ($ARGV[1] =~ /^users-?import$/) {
    open(my $fh, "<", $ARGV[2]) or die "cannot open < $ARGV[2]: $!";
    my $data = do { local $/; <$fh> };
    esPost("/_bulk", $data);
    close($fh);
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
    showHelp("Invalid expire <type>") if ($ARGV[2] !~ /^(hourly|daily|weekly|monthly)$/);

    # First handle sessions expire
    my $indices = esGet("/${PREFIX}sessions-*/_aliases", 1);

    my $endTime = time();
    my $endTimeIndex = time2index($ARGV[2], "sessions-", $endTime);
    delete $indices->{$endTimeIndex};

    my @startTime = gmtime;
    if ($ARGV[2] eq "hourly") {
        $startTime[2] -= int($ARGV[3]);
    } elsif ($ARGV[2] eq "daily") {
        $startTime[3] -= int($ARGV[3]);
    } elsif ($ARGV[2] eq "weekly") {
        $startTime[3] -= 7*int($ARGV[3]);
    } elsif ($ARGV[2] eq "monthly") {
        $startTime[4] -= int($ARGV[3]);
    }

    parseArgs(4);

    my $optimizecnt = 0;
    my $startTime = mktime(@startTime);
    while ($startTime <= $endTime) {
        my $iname = time2index($ARGV[2], "sessions-", $startTime);
        if (exists $indices->{$iname} && $indices->{$iname}->{OPTIMIZEIT} != 1) {
            $indices->{$iname}->{OPTIMIZEIT} = 1;
            $optimizecnt++;
        }
        if ($ARGV[2] eq "hourly") {
            $startTime += 60*60;
        } else {
            $startTime += 24*60*60;
        }
    }

    dbESVersion();
    $main::userAgent->timeout(3600);
    optimizeOther() unless $NOOPTIMIZE ;
    printf ("Expiring %s sessions indices, %s optimizing %s\n", commify(scalar(keys %{$indices}) - $optimizecnt), $NOOPTIMIZE?"Not":"", commify($optimizecnt));
    foreach my $i (sort (keys %{$indices})) {
        progress("$i ");
        if (exists $indices->{$i}->{OPTIMIZEIT}) {
            esGet("/$i/$main::OPTIMIZE?max_num_segments=4", 1) unless $NOOPTIMIZE ;
            if ($REPLICAS != -1) {
                esGet("/$i/_flush", 1);
                esPut("/$i/_settings", '{index: {"number_of_replicas":' . $REPLICAS . '}}', 1);
            }
        } else {
            esDelete("/$i", 1);
        }
    }

    # Now figure out history expire
    my $hindices = esGet("/${PREFIX}history_v1-*/_aliases", 1);

    $endTimeIndex = time2index("weekly", "history_v1-", $endTime);
    delete $hindices->{$endTimeIndex};

    @startTime = gmtime;
    $startTime[3] -= 7 * $HISTORY;

    $optimizecnt = 0;
    $startTime = mktime(@startTime);
    while ($startTime <= $endTime) {
        my $iname = time2index("weekly", "history_v1-", $startTime);
        if (exists $hindices->{$iname} && $hindices->{$iname}->{OPTIMIZEIT} != 1) {
            $hindices->{$iname}->{OPTIMIZEIT} = 1;
            $optimizecnt++;
        }
        $startTime += 24*60*60;
    }

    printf ("Expiring %s history indices, %s optimizing %s\n", commify(scalar(keys %{$hindices}) - $optimizecnt), $NOOPTIMIZE?"Not":"", commify($optimizecnt));
    foreach my $i (sort (keys %{$hindices})) {
        progress("$i ");
        if (exists $hindices->{$i}->{OPTIMIZEIT}) {
            esGet("/$i/$main::OPTIMIZE?max_num_segments=1", 1) unless $NOOPTIMIZE ;
        } else {
            esDelete("/$i", 1);
        }
    }
    exit 0;
} elsif ($ARGV[1] eq "optimize") {
    my $indices = esGet("/${PREFIX}sessions-*/_aliases", 1);

    dbESVersion();
    $main::userAgent->timeout(3600);
    optimizeOther();
    printf "Optimizing %s Session Indices\n", commify(scalar(keys %{$indices}));
    foreach my $i (sort (keys %{$indices})) {
        progress("$i ");
        esGet("/$i/$main::OPTIMIZE?max_num_segments=4", 1);
        esPost("/$i/_upgrade", "", 1);
    }
    print "\n";
    exit 0;
} elsif ($ARGV[1] eq "info") {
    dbVersion(0);
    my $esversion = dbESVersion();
    my $nodes = esGet("/_nodes");
    my $status = esGet("/_stats", 1);

    my $sessions = 0;
    my $sessionsBytes = 0;
    my @sessions = grep /^${PREFIX}sessions-/, keys %{$status->{indices}};
    foreach my $index (@sessions) {
        next if ($index !~ /^${PREFIX}sessions-/);
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
        printf "%-20s %10s (%s bytes)\n", $name . ":", commify($index->{primaries}->{docs}->{count}), commify($index->{primaries}->{store}->{size_in_bytes});
    }

    printf "ES Version:          %10s\n", $esversion->{version}->{number};
    printf "DB Version:          %10s\n", $main::versionNumber;
    printf "ES Nodes:            %10s/%s\n", commify(dataNodes($nodes->{nodes})), commify(scalar(keys %{$nodes->{nodes}}));
    printf "Session Indices:     %10s\n", commify(scalar(@sessions));
    printf "Sessions:            %10s (%s bytes)\n", commify($sessions), commify($sessionsBytes);
    if (scalar(@sessions) > 0) {
        printf "Session Density:     %10s (%s bytes)\n", commify(int($sessions/(scalar(keys %{$nodes->{nodes}})*scalar(@sessions)))),
                                                       commify(int($sessionsBytes/(scalar(keys %{$nodes->{nodes}})*scalar(@sessions))));
    }
    printf "History Indices:     %10s\n", commify(scalar(@historys));
    printf "Histories:           %10s (%s bytes)\n", commify($historys), commify($historysBytes);
    if (scalar(@historys) > 0) {
        printf "History Density:     %10s (%s bytes)\n", commify(int($historys/(scalar(keys %{$nodes->{nodes}})*scalar(@historys)))),
                                                       commify(int($historysBytes/(scalar(keys %{$nodes->{nodes}})*scalar(@historys))));
    }
    printIndex($status, "files_v4");
    printIndex($status, "files_v3");
    printIndex($status, "tags_v3");
    printIndex($status, "tags_v2");
    printIndex($status, "users_v4");
    printIndex($status, "users_v3");
    exit 0;
} elsif ($ARGV[1] eq "mv") {
    (my $fn = $ARGV[2]) =~ s/\//\\\//g;
    my $results = esGet("/${PREFIX}files/_search?q=name:$fn");
    die "Couldn't find '$ARGV[2]' in db\n" if (@{$results->{hits}->{hits}} == 0);

    foreach my $hit (@{$results->{hits}->{hits}}) {
        my $script = '{"script" : "ctx._source.name = \"' . $ARGV[3] . '\"; ctx._source.locked = 1;"}';
        esPost("/${PREFIX}files/file/" . $hit->{_id} . "/_update", $script);
    }
    print "Moved " . scalar (@{$results->{hits}->{hits}}) . " file(s) in database\n";
    exit 0;
} elsif ($ARGV[1] eq "rm") {
    (my $fn = $ARGV[2]) =~ s/\//\\\//g;
    my $results = esGet("/${PREFIX}files/_search?q=name:$fn");
    die "Couldn't find '$ARGV[2]' in db\n" if (@{$results->{hits}->{hits}} == 0);

    foreach my $hit (@{$results->{hits}->{hits}}) {
        esDelete("/${PREFIX}files/file/" . $hit->{_id}, 0);
    }
    print "Removed " . scalar (@{$results->{hits}->{hits}}) . " file(s) in database\n";
    exit 0;
} elsif ($ARGV[1] =~ /^rm-?missing$/) {
    my $results = esGet("/${PREFIX}files/_search?size=10000&q=node:$ARGV[2]");
    die "Couldn't find '$ARGV[2]' in db\n" if (@{$results->{hits}->{hits}} == 0);
    print "Need to remove references to these files from database:\n";
    my $cnt = 0;
    foreach my $hit (@{$results->{hits}->{hits}}) {
        if (! -f $hit->{_source}->{name}) {
            print $hit->{_source}->{name}, "\n";
            $cnt++;
        }
    }
    die "Nothing found to remove." if ($cnt == 0);
    print "\n";
    waitFor("YES", "Do you want to remove file references from database?");
    foreach my $hit (@{$results->{hits}->{hits}}) {
        if (! -f $hit->{_source}->{name}) {
            esDelete("/${PREFIX}files/file/" . $hit->{_id}, 0);
        }
    }
    exit 0;
} elsif ($ARGV[1] =~ /^rm-?node$/) {
    my $results = esGet("/${PREFIX}files/_search?size=10000&q=node:$ARGV[2]");
    print "Deleting ", $results->{hits}->{total}, " files\n";
    foreach my $hit (@{$results->{hits}->{hits}}) {
        esDelete("/${PREFIX}files/file/" . $hit->{_id}, 0);
    }
    esDelete("/${PREFIX}stats/stat/" . $ARGV[2], 1);
    my $results = esGet("/${PREFIX}dstats/_search?size=10000&q=nodeName:$ARGV[2]");
    print "Deleting ", $results->{hits}->{total}, " stats\n";
    foreach my $hit (@{$results->{hits}->{hits}}) {
        esDelete("/${PREFIX}dstats/dstat/" . $hit->{_id}, 0);
    }
    exit 0;
} elsif ($ARGV[1] =~ /^add-?missing$/) {
    my $dir = $ARGV[3];
    chop $dir if (substr($dir, -1) eq "/");
    opendir(my $dh, $dir) || die "Can't opendir $dir: $!";
    my @files = grep { m/^$ARGV[2]-/ && -f "$dir/$_" } readdir($dh);
    closedir $dh;
    print "Checking ", scalar @files, " files, this may take a while.\n";
    foreach my $file (@files) {
        $file =~ /(\d+)-(\d+).pcap/;
        my $filenum = int($2);
        my $ctime = (stat("$dir/$file"))[10];
        my $info = esGet("/${PREFIX}files/file/$ARGV[2]-$filenum", 1);
        if (!$info->{found}) {
            print "Adding $dir/$file $filenum $ctime\n";
            esPost("/${PREFIX}files/file/$ARGV[2]-$filenum", to_json({
                         'locked' => 0,
                         'first' => $ctime,
                         'num' => $filenum,
                         'name' => "$dir/$file",
                         'node' => $ARGV[2]}), 1);
        } elsif ($verbose > 0) {
            print "Ok $dir/$file\n";
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
    my %remotefileshash;
    foreach my $hit (@{$remotefiles}) {
        if (! -f $hit->{_source}->{name}) {
            progress("Removing " . $hit->{_source}->{name} . " id: " . $hit->{_id}, "\n");
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
    print("\n") if ($verbose > 0);
    exit 0;
} elsif ($ARGV[1] =~ /^(field)$/) {
    my $result = esGet("/${PREFIX}fields/field/$ARGV[3]", 1);
    my $found = $result->{found};
    die "Field $ARGV[3] isn't found" if (!$found);

    esPost("/${PREFIX}fields/field/$ARGV[3]/_update", "{\"doc\":{\"disabled\":" . ($ARGV[2] eq "disable"?"true":"false").  "}}");
    exit 0;
} elsif ($ARGV[1] =~ /^force-?put-?version$/) {
    esPost("/${PREFIX}dstats/version/version", "{\"version\": $VERSION}");
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
print "It is STRONGLY recommended that you stop ALL moloch captures and viewers before proceeding.\n\n";
if ($main::numberOfNodes == 1) {
    print "There is $main::numberOfNodes elastic search data node, if you expect more please fix first before proceeding.\n\n";
} else {
    print "There are $main::numberOfNodes elastic search data nodes, if you expect more please fix first before proceeding.\n\n";
}

if (int($SHARDS) > $main::numberOfNodes) {
    die "Can't set shards ($SHARDS) greater then the number of nodes ($main::numberOfNodes)";
} elsif ($SHARDS == -1) {
    $SHARDS = $main::numberOfNodes;
}

dbVersion(1);

if ($ARGV[1] eq "wipe" && $main::versionNumber != $VERSION) {
    die "Can only use wipe if schema is up to date.  Use upgrade first.";
}

dbCheck();

if ($ARGV[1] =~ /(init|wipe)/) {

    if ($ARGV[1] eq "init" && $main::versionNumber >= 0) {
        print "It appears this elastic search cluster already has moloch installed, this will delete ALL data in elastic search! (It does not delete the pcap files on disk.)\n\n";
        waitFor("INIT", "do you want to erase everything?");
    } elsif ($ARGV[1] eq "wipe") {
        print "This will delete ALL session data in elastic search! (It does not delete the pcap files on disk or user info.)\n\n";
        waitFor("WIPE", "do you want to wipe everything?");
    }
    print "Erasing\n";
    esDelete("/${PREFIX}tags_v3", 1);
    esDelete("/${PREFIX}tags_v2", 1);
    esDelete("/${PREFIX}tags", 1);
    esDelete("/${PREFIX}sequence", 1);
    esDelete("/${PREFIX}sequence_v1", 1);
    esDelete("/${PREFIX}files_v4", 1);
    esDelete("/${PREFIX}files_v3", 1);
    esDelete("/${PREFIX}files", 1);
    esDelete("/${PREFIX}stats", 1);
    esDelete("/${PREFIX}stats_v1", 1);
    esDelete("/${PREFIX}stats_v2", 1);
    esDelete("/${PREFIX}dstats", 1);
    esDelete("/${PREFIX}fields", 1);
    esDelete("/${PREFIX}dstats_v1", 1);
    esDelete("/${PREFIX}dstats_v2", 1);
    esDelete("/${PREFIX}sessions-*", 1);
    esDelete("/_template/${PREFIX}template_1", 1);
    esDelete("/_template/${PREFIX}sessions_template", 1);
    esDelete("/${PREFIX}fields", 1);
    esDelete("/${PREFIX}fields_v1", 1);
    esDelete("/${PREFIX}history_v1-*", 1);
    if ($ARGV[1] =~ "init") {
        esDelete("/${PREFIX}users_v3", 1);
        esDelete("/${PREFIX}users_v4", 1);
        esDelete("/${PREFIX}users", 1);
        esDelete("/${PREFIX}queries", 1);
        esDelete("/${PREFIX}queries_v1", 1);
    }
    esDelete("/${PREFIX}tagger", 1);

    sleep(1);

    print "Creating\n";
    tagsCreate();
    sequenceCreate();
    filesCreate();
    statsCreate();
    dstatsCreate();
    sessionsUpdate();
    fieldsCreate();
    historyUpdate();
    if ($ARGV[1] =~ "init") {
        usersCreate();
        queriesCreate();
    }
    print "Finished.  Have fun!\n";
} else {

# Remaing is upgrade or upgradenoprompt

# For really old versions don't support upgradenoprompt
    if ($main::versionNumber < 19) {
        print "No longer supported.  Please upgrade to Moloch 0.17.0 first. (Db version $main::VersionNumber)\n\n";
        exit 1;
    }

    if ($health->{status} eq "red") {
        print "Not auto upgrading when elasticsearch status is red.\n\n";
        waitFor("RED", "do you want to really want to upgrade?");
    } elsif ($ARGV[1] ne "upgradenoprompt") {
        print "Trying to upgrade from version $main::versionNumber to version $VERSION.\n\n";
        waitFor("UPGRADE", "do you want to upgrade?");
    }

    print "Starting Upgrade\n";

    if ($main::versionNumber <= 31) {
        dbCheckForActivity();
        esGet("/_flush", 0);
        esGet("/_refresh", 0);

        sequenceUpgrade();

        if ($main::versionNumber < 20) {
            queriesCreate();
        } else {
            createAliasedFromNonAliased("queries", "queries_v1", \&queriesCreate);
        }

        esDelete("/${PREFIX}tags_v1", 1);
        createAliasedFromNonAliased("fields", "fields_v1", \&fieldsCreate);
        createNewAliasesFromOld("tags", "tags_v3", "tags_v2", \&tagsCreate);

        esDelete("/${PREFIX}users_v1", 1);
        esDelete("/${PREFIX}users_v2", 1);
        createNewAliasesFromOld("users", "users_v4", "users_v3", \&usersCreate);

        esDelete("/${PREFIX}files_v1", 1);
        esDelete("/${PREFIX}files_v2", 1);
        createNewAliasesFromOld("files", "files_v4", "files_v3", \&filesCreate);

        if ($main::versionNumber <= 30) {
            createNewAliasesFromOld("dstats", "dstats_v2", "dstats_v1", \&dstatsCreate);
            createAliasedFromNonAliased("stats", "stats_v2", \&statsCreate);
        }

        esDelete("/_template/${PREFIX}template_1", 1);
        historyUpdate();
        sessionsUpdate();
        checkForOldIndices();
    } elsif ($main::versionNumber <= 33) {
        createNewAliasesFromOld("stats", "stats_v2", "stats_v1", \&statsCreate);
        usersUpdate();
        historyUpdate();
        sessionsUpdate();
        checkForOldIndices();
    } elsif ($main::versionNumber <= 37) {
        usersUpdate();
        historyUpdate();
        sessionsUpdate();
        checkForOldIndices();
    } else {
        print "db.pl is hosed\n";
    }

}

print "Finished\n";

sleep 1;
esPost("/${PREFIX}dstats/version/version", "{\"version\": $VERSION}");
