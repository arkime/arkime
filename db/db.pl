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

use HTTP::Request::Common;
use LWP::UserAgent;
use JSON;
use Data::Dumper;
use POSIX;
use strict;

my $VERSION = 25;
my $verbose = 0;
my $PREFIX = "";

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
    print "$0 [Options] <ESHOST:ESPORT> <command> [<options>]\n";
    print "\n";
    print "Options:\n";
    print "  -v                    - Verbose, multiple increases level\n";
    print "  --prefix <prefix>     - Prefix for table names\n";
    print "\n";
    print "Commands:\n";
    print "  init                  - Clear ALL elasticsearch moloch data and create schema\n";
    print "  wipe                  - Same as init, but leaves user database untouched\n";
    print "  upgrade               - Upgrade Moloch's schema in elasticsearch from previous versions\n";
    print "  info                  - Information about the database\n";
    print "  users-export <fn>     - Save the users info to <fn>\n";
    print "  users-import <fn>     - Load the users info from <fn>\n";
    print "  optimize              - Optimize all indices in ES\n";
    print "  mv <old fn> <new fn>  - Move a pcap file in the database (doesn't change disk)\n";
    print "  rm <fn>               - Remove a pcap file in the database (doesn't change disk)\n";
    print "  rm-missing <node>     - Remove from db any MISSING file on THIS machine for named node\n";
    print "  rm-node <node>        - Remove from db all data for node (doesn't change disk)\n";
    print "  expire <type> <num>   - Perform daily maintenance and optimize all indices in ES\n";
    print "       type             - Same as rotateIndex in ini file = hourly,daily,weekly,monthly\n";
    print "       num              - number of indexes to keep\n";
    print "  field disable <exp >  - disable a field from being indexed\n";
    print "  field enable <exp >   - enable a field from being indexed\n";
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
    print "GET http://$ARGV[0]$url\n" if ($verbose > 2);
    my $response = $main::userAgent->get("http://$ARGV[0]$url");
    if (($response->code == 500 && $ARGV[1] ne "init") || ($response->code != 200 && !$dontcheck)) {
      die "Couldn't GET http://$ARGV[0]$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?";
    }
    my $json = from_json($response->content);
    return $json
}

################################################################################
sub esPost
{
    my ($url, $content, $dontcheck) = @_;
    print "POST http://$ARGV[0]$url\n" if ($verbose > 2);
    my $response = $main::userAgent->post("http://$ARGV[0]$url", Content => $content);
    if ($response->code == 500 || ($response->code != 200 && $response->code != 201 && !$dontcheck)) {
      die "Couldn't POST http://$ARGV[0]$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?";
    }

    my $json = from_json($response->content);
    return $json
}

################################################################################
sub esPut
{
    my ($url, $content, $dontcheck) = @_;
    print "PUT http://$ARGV[0]$url\n" if ($verbose > 2);
    my $response = $main::userAgent->request(HTTP::Request::Common::PUT("http://$ARGV[0]$url", Content => $content));
    if ($response->code == 500 || ($response->code != 200 && !$dontcheck)) {
      print Dumper($response);
      die "Couldn't PUT http://$ARGV[0]$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?\n" . $response->content;
    }
    my $json = from_json($response->content);
    return $json
}

################################################################################
sub esDelete
{
    my ($url, $dontcheck) = @_;
    print "DELETE http://$ARGV[0]$url\n" if ($verbose > 2);
    my $response = $main::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$ARGV[0]$url"));
    if ($response->code == 500 || ($response->code != 200 && !$dontcheck)) {
      die "Couldn't DELETE http://$ARGV[0]$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?";
    }
    my $json = from_json($response->content);
    return $json
}

################################################################################
sub esCopy
{
    my ($srci, $dsti, $type) = @_;

    my $status = esGet("/_status", 1);
    print "Copying " . $status->{indices}->{$PREFIX . $srci}->{docs}->{num_docs} . " elements from ${PREFIX}$srci/$type to ${PREFIX}$dsti/$type\n";

    my $id = "";
    while (1) {
        if ($verbose > 0) {
            local $| = 1;
            print ".";
        }
        my $url;
        if ($id eq "") {
            $url = "/${PREFIX}$srci/$type/_search?scroll=10m&scroll_id=$id&size=500";
        } else {
            $url = "/_search/scroll?scroll=10m&scroll_id=$id";
        }
        

        my $incoming = esGet($url);
        my $out = "";
        last if (@{$incoming->{hits}->{hits}} == 0);

        foreach my $hit (@{$incoming->{hits}->{hits}}) {
            $out .= "{\"index\": {\"_index\": \"${PREFIX}$dsti\", \"_type\": \"$type\", \"_id\": \"" . $hit->{_id} . "\"}}\n";
            $out .= to_json($hit->{_source}) . "\n";
        }

        $id = $incoming->{_scroll_id};

        esPost("/_bulk", $out);
    }
    print "\n"
}
################################################################################
sub esAlias
{
    my ($cmd, $index, $alias) = @_;

    print "Alias cmd $cmd from $index to alias $alias\n" if ($verbose > 0);
    esPost("/_aliases", '{ actions: [ { ' . $cmd . ': { index: "' . $PREFIX . $index . '", alias : "'. $PREFIX . $alias .'" } } ] }', 1);
}

################################################################################
sub tagsCreate
{
    my $settings = '
{
  settings: {
    number_of_shards: 1,
    number_of_replicas: 0,
    auto_expand_replicas: "0-all"
  }
}';

    print "Creating tags_v2 index\n" if ($verbose > 0);
    esPut("/${PREFIX}tags_v2/", $settings);
    esAlias("add", "tags_v2", "tags");
    tagsUpdate();
}

################################################################################
sub tagsUpdate
{
    my $mapping = '
{
  tag: {
    _id: { index: "not_analyzed"},
    dynamic: "strict",
    properties: {
      n: {
        type: "integer"
      }
    }
  }
}';

    print "Setting tags_v2 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}tags_v2/tag/_mapping", $mapping);
}
################################################################################
sub sequenceCreate
{
    my $settings = '
{
  settings: {
    number_of_shards: 1,
    number_of_replicas: 0,
    auto_expand_replicas: "0-all"
  }
}';

    print "Creating sequence index\n" if ($verbose > 0);
    esPut("/${PREFIX}sequence", $settings);
    sequenceUpdate();
}

################################################################################
sub sequenceUpdate
{
    my $mapping = '
{
  sequence: {
    _source : { enabled : 0 },
    _all    : { enabled : 0 },
    _type   : { index : "no" },
    enabled : 0
  }
}';

    print "Setting sequence mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}sequence/sequence/_mapping", $mapping);
}
################################################################################
sub filesCreate
{
    my $settings = '
{
  settings: {
    number_of_shards: 2,
    number_of_replicas: 0,
    auto_expand_replicas: "0-2"
  }
}';

    print "Creating files_v3 index\n" if ($verbose > 0);
    esPut("/${PREFIX}files_v3", $settings);
    esAlias("add", "files_v3", "files");
    filesUpdate();
}
################################################################################
sub filesUpdate
{
    my $mapping = '
{
  file: {
    _all : {enabled : 0},
    _source : {enabled : 1},
    dynamic: "strict",
    properties: {
      num: {
        type: "long",
        index: "not_analyzed"
      },
      node: {
        type: "string",
        index: "not_analyzed"
      },
      first: {
        type: "long",
        index: "not_analyzed"
      },
      name: {
        type: "string",
        index: "not_analyzed"
      },
      filesize: {
        type: "long"
      },
      locked: {
        type: "short",
        index: "not_analyzed"
      },
      last: {
        type: "long",
        index: "not_analyzed"
      }
    }
  }
}';

    print "Setting files_v3 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}files_v3/file/_mapping", $mapping);
}
################################################################################
sub statsCreate
{
    my $settings = '
{
  index: {
    store: {
      type: "memory"
    }
  },
  settings: {
      number_of_shards: 1,
      number_of_replicas: 0,
      auto_expand_replicas: "0-all"
  }
}';

    print "Creating stats index\n" if ($verbose > 0);
    esPut("/${PREFIX}stats", $settings);
    statsUpdate();
}

################################################################################
sub statsUpdate
{
my $mapping = '
{
  stat: {
    _all : {enabled : false},
    _source : {enabled : true},
    dynamic: "strict",
    properties: {
      hostname: {
        type: "string",
        index: "not_analyzed"
      },
      currentTime: {
        type: "long"
      },
      freeSpaceM: {
        type: "long",
        index: "no"
      },
      totalK: {
        type: "long",
        index: "no"
      },
      totalPackets: {
        type: "long",
        index: "no"
      },
      monitoring: {
        type: "long",
        index: "no"
      },
      totalSessions: {
        type: "long",
        index: "no"
      },
      totalDropped: {
        type: "long",
        index: "no"
      },
      deltaMS: {
        type: "long",
        index: "no"
      },
      deltaBytes: {
        type: "long",
        index: "no"
      },
      deltaPackets: {
        type: "long",
        index: "no"
      },
      deltaSessions: {
        type: "long",
        index: "no"
      },
      deltaDropped: {
        type: "long",
        index: "no"
      },
      memory: {
        type: "long",
        index: "no"
      },
      cpu: {
        type: "integer",
        index: "no"
      },
      diskQueue: {
        type: "long",
        index: "no"
      }
    }
  }
}';

    print "Setting stats mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}stats/stat/_mapping?pretty&ignore_conflicts=true", $mapping, 1);
}
################################################################################
sub dstatsCreate
{
    my $settings = '
{
  settings: {
    number_of_shards: 2,
    number_of_replicas: 0,
    auto_expand_replicas: "0-2"
  }
}';

    print "Creating dstats_v1 index\n" if ($verbose > 0);
    esPut("/${PREFIX}dstats_v1", $settings);
    esAlias("add", "dstats_v1", "dstats");
    dstatsUpdate();
}

################################################################################
sub dstatsUpdate
{
my $mapping = '
{
  dstat: {
    _all : {enabled : false},
    _source : {enabled : true},
    dynamic: "strict",
    properties: {
      nodeName: {
        type: "string",
        index: "not_analyzed"
      },
      interval: {
        type: "short"
      },
      currentTime: {
        type: "long"
      },
      freeSpaceM: {
        type: "long",
        index: "no"
      },
      deltaMS: {
        type: "long",
        index: "no"
      },
      deltaBytes: {
        type: "long",
        index: "no"
      },
      deltaPackets: {
        type: "long",
        index: "no"
      },
      deltaSessions: {
        type: "long",
        index: "no"
      },
      deltaDropped: {
        type: "long",
        index: "no"
      },
      monitoring: {
        type: "long",
        index: "no"
      },
      memory: {
        type: "long",
        index: "no"
      },
      cpu: {
        type: "integer",
        index: "no"
      },
      diskQueue: {
        type: "long",
        index: "no"
      }
    }
  }
}';

    print "Setting dstats_v1 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}dstats_v1/dstat/_mapping?pretty&ignore_conflicts=true", $mapping, 1);
}
################################################################################
sub fieldsCreate
{
    my $settings = '
{
  settings: {
    number_of_shards: 1,
    number_of_replicas: 0,
    auto_expand_replicas: "0-2"
  }
}';

    print "Creating fields index\n" if ($verbose > 0);
    esPut("/${PREFIX}fields", $settings);
    fieldsUpdate();
}
################################################################################
sub fieldsUpdate
{
    my $mapping = '
{
  field: {
    _all : {enabled : 0},
    _source : {enabled : 1},
    dynamic_templates: [
      {
        template_1: {
          match_mapping_type: "string",
          mapping: {
            index: "not_analyzed"
          }
        }
      }
    ]
  }
}';

    print "Setting fields mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}fields/field/_mapping", $mapping);

    esPost("/${PREFIX}fields/field/ip", '{
      "friendlyName": "All IP fields",
      "group": "general",
      "help": "Search all ip fields",
      "type": "ip",
      "dbField": "ipall",
      "portField": "portall",
      "noFacet": true
    }');
    esPost("/${PREFIX}fields/field/port", '{
      "friendlyName": "All port fields",
      "group": "general",
      "help": "Search all port fields",
      "type": "integer",
      "dbField": "portall",
      "regex": "(^port\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.port$)"
    }');
    esPost("/${PREFIX}fields/field/rir", '{
      "friendlyName": "All rir fields",
      "group": "general",
      "help": "Search all rir fields",
      "type": "uptermfield",
      "dbField": "all",
      "regex": "(^rir\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.rir$)"
    }');
    esPost("/${PREFIX}fields/field/country", '{
      "friendlyName": "All country fields",
      "group": "general",
      "help": "Search all country fields",
      "type": "uptermfield",
      "dbField": "all",
      "regex": "(^country\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.country$)"
    }');
    esPost("/${PREFIX}fields/field/asn", '{
      "friendlyName": "All ASN fields",
      "group": "general",
      "help": "Search all ASN fields",
      "type": "textfield",
      "dbField": "all",
      "regex": "(^asn\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.asn$)"
    }');
    esPost("/${PREFIX}fields/field/host", '{
      "friendlyName": "All Host fields",
      "group": "general",
      "help": "Search all Host fields",
      "type": "lotextfield",
      "dbField": "all",
      "regex": "(^host\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.host$)"
    }');
    esPost("/${PREFIX}fields/field/ip.src", '{
      "friendlyName": "Src IP",
      "group": "general",
      "help": "Source IP",
      "type": "ip",
      "dbField": "a1",
      "portField": "p1",
      "category": "ip"
    }');
    esPost("/${PREFIX}fields/field/port.src", '{
      "friendlyName": "Src Port",
      "group": "general",
      "help": "Source Port",
      "type": "integer",
      "dbField": "p1",
      "category": "port"
    }');
    esPost("/${PREFIX}fields/field/asn.src", '{
      "friendlyName": "Src ASN",
      "group": "general",
      "help": "GeoIP ASN string calculated from the source IP",
      "type": "textfield",
      "dbField": "as1",
      "rawField": "rawas1",
      "category": "asn"
    }');
    esPost("/${PREFIX}fields/field/country.src", '{
      "friendlyName": "Src Country",
      "group": "general",
      "help": "Source Country",
      "type": "uptermfield",
      "dbField": "g1",
      "category": "country"
    }');
    esPost("/${PREFIX}fields/field/rir.src", '{
      "friendlyName": "Src RIR",
      "group": "general",
      "help": "Source RIR",
      "type": "uptermfield",
      "dbField": "rir1",
      "category": "rir"
    }');
    esPost("/${PREFIX}fields/field/ip.dst", '{
      "friendlyName": "Dst IP",
      "group": "general",
      "help": "Destination IP",
      "type": "ip",
      "dbField": "a2",
      "portField": "p2",
      "category": "ip"
    }');
    esPost("/${PREFIX}fields/field/port.dst", '{
      "friendlyName": "Dst Port",
      "group": "general",
      "help": "Source Port",
      "type": "integer",
      "dbField": "p2",
      "category": "port"
    }');
    esPost("/${PREFIX}fields/field/asn.dst", '{
      "friendlyName": "Dst ASN",
      "group": "general",
      "help": "GeoIP ASN string calculated from the destination IP",
      "type": "textfield",
      "dbField": "as2",
      "rawField": "rawas2",
      "category": "asn"
    }');
    esPost("/${PREFIX}fields/field/country.dst", '{
      "friendlyName": "Dst Country",
      "group": "general",
      "help": "Destination Country",
      "type": "uptermfield",
      "dbField": "g2",
      "category": "country"
    }');
    esPost("/${PREFIX}fields/field/rir.dst", '{
      "friendlyName": "Dst RIR",
      "group": "general",
      "help": "Destination RIR",
      "type": "uptermfield",
      "dbField": "rir2",
      "category": "rir"
    }');
    esPost("/${PREFIX}fields/field/bytes", '{
      "friendlyName": "Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent AND received in a session",
      "type": "integer",
      "dbField": "by"
    }');
    esPost("/${PREFIX}fields/field/bytes.src", '{
      "friendlyName": "Src Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent by source in a session",
      "type": "integer",
      "dbField": "by1"
    }');
    esPost("/${PREFIX}fields/field/bytes.dst", '{
      "friendlyName": "Dst Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent by destination in a session",
      "type": "integer",
      "dbField": "by2"
    }');
    esPost("/${PREFIX}fields/field/databytes", '{
      "friendlyName": "Data bytes",
      "group": "general",
      "help": "Total number of data bytes sent AND received in a session",
      "type": "integer",
      "dbField": "db"
    }');
    esPost("/${PREFIX}fields/field/databytes.src", '{
      "friendlyName": "Src data bytes",
      "group": "general",
      "help": "Total number of data bytes sent by source in a session",
      "type": "integer",
      "dbField": "db1"
    }');
    esPost("/${PREFIX}fields/field/databytes.dst", '{
      "friendlyName": "Dst data bytes",
      "group": "general",
      "help": "Total number of data bytes sent by destination in a session",
      "type": "integer",
      "dbField": "db2"
    }');
    esPost("/${PREFIX}fields/field/packets", '{
      "friendlyName": "Packets",
      "group": "general",
      "help": "Total number of packets sent AND received in a session",
      "type": "integer",
      "dbField": "pa"
    }');
    esPost("/${PREFIX}fields/field/packets.src", '{
      "friendlyName": "Src Packets",
      "group": "general",
      "help": "Total number of packets sent by source in a session",
      "type": "integer",
      "dbField": "pa1"
    }');
    esPost("/${PREFIX}fields/field/packets.dst", '{
      "friendlyName": "Dst Packets",
      "group": "general",
      "help": "Total number of packets sent by destination in a session",
      "type": "integer",
      "dbField": "pa2"
    }');
    esPost("/${PREFIX}fields/field/ip.protocol", '{
      "friendlyName": "IP Protocol",
      "group": "general",
      "help": "IP protocol number or friendly name",
      "type": "lotermfield",
      "dbField": "pr",
      "transform": "ipProtocolLookup"
    }');
    esPost("/${PREFIX}fields/field/id", '{
      "friendlyName": "Moloch ID",
      "group": "general",
      "help": "Moloch ID for the session",
      "type": "termfield",
      "dbField": "_id",
      "noFacet": true

    }');
    esPost("/${PREFIX}fields/field/rootId", '{
      "friendlyName": "Moloch Root ID",
      "group": "general",
      "help": "Moloch ID of the first session in a multi session stream",
      "type": "termfield",
      "dbField": "ro"
    }');
    esPost("/${PREFIX}fields/field/node", '{
      "friendlyName": "Moloch Node",
      "group": "general",
      "help": "Moloch node name the session was recorded on",
      "type": "termfield",
      "dbField": "no"
    }');
    esPost("/${PREFIX}fields/field/file", '{
      "friendlyName": "Filename",
      "group": "general",
      "help": "Moloch offline pcap filename",
      "type": "fileand",
      "dbField": "fileand"
    }');
    esPost("/${PREFIX}fields/field/payload8.src.hex", '{
      "friendlyName": "Payload Src Hex",
      "group": "general",
      "help": "First 8 bytes of source payload in hex",
      "type": "lotermfield",
      "dbField": "fb1",
      "aliases": ["payload.src"]
    }');
    esPost("/${PREFIX}fields/field/payload8.src.utf8", '{
      "friendlyName": "Payload Src UTF8",
      "group": "general",
      "help": "First 8 bytes of source payload in utf8",
      "type": "termfield",
      "dbField": "fb1",
      "transform": "utf8ToHex",
      "noFacet": true
    }');
    esPost("/${PREFIX}fields/field/payload8.dst.hex", '{
      "friendlyName": "Payload Dst Hex",
      "group": "general",
      "help": "First 8 bytes of destination payload in hex",
      "type": "lotermfield",
      "dbField": "fb2",
      "aliases": ["payload.dst"]
    }');
    esPost("/${PREFIX}fields/field/payload8.dst.utf8", '{
      "friendlyName": "Payload Dst UTF8",
      "group": "general",
      "help": "First 8 bytes of destination payload in utf8",
      "type": "termfield",
      "dbField": "fb2",
      "transform": "utf8ToHex",
      "noFacet": true
    }');
    esPost("/${PREFIX}fields/field/payload8.hex", '{
      "friendlyName": "Payload Hex",
      "group": "general",
      "help": "First 8 bytes of payload in hex",
      "type": "lotermfield",
      "dbField": "fballhex",
      "regex": "^payload8.(src|dst).hex$"
    }');
    esPost("/${PREFIX}fields/field/payload8.utf8", '{
      "friendlyName": "Payload UTF8",
      "group": "general",
      "help": "First 8 bytes of payload in hex",
      "type": "lotermfield",
      "dbField": "fballutf8",
      "regex": "^payload8.(src|dst).utf8$"
    }');
    esPost("/${PREFIX}fields/field/scrubbed.by", '{
      "friendlyName": "Scrubbed By",
      "group": "general",
      "help": "SPI data was scrubbed by",
      "type": "lotermfield",
      "dbField": "scrubby"
    }');
    esPost("/${PREFIX}fields/field/view", '{
      "friendlyName": "View Name",
      "group": "general",
      "help": "Moloch view name",
      "type": "viewand",
      "dbField": "viewand",
      "noFacet": true
    }');
    esPost("/${PREFIX}fields/field/starttime", '{
      "friendlyName": "Start Time",
      "group": "general",
      "help": "Session Start Time",
      "type": "seconds",
      "dbField": "fp"
    }');
    esPost("/${PREFIX}fields/field/stoptime", '{
      "friendlyName": "Stop Time",
      "group": "general",
      "help": "Session Stop Time",
      "type": "seconds",
      "dbField": "lp"
    }');

    esPost("/${PREFIX}fields/field/dns.status/_update", '{doc: {type: "uptermfield"}}', 1);
    esPost("/${PREFIX}fields/field/http.hasheader/_update", '{doc: {regex: "^http.hasheader\\\\.(?:(?!\\\\.cnt$).)*$"}}', 1);
    esPost("/${PREFIX}fields/field/email.subject/_update", '{doc: {type: "textfield"}}', 1);
}

################################################################################
sub queriesCreate
{
    my $settings = '
{
  settings: {
    number_of_shards: 1,
    number_of_replicas: 0,
    auto_expand_replicas: "0-2"
  }
}';

    print "Creating queries index\n" if ($verbose > 0);
    esPut("/${PREFIX}queries", $settings);
    queriesUpdate();
}
################################################################################
sub queriesUpdate
{
    my $mapping = '
{
  query: {
    _all : {enabled : 0},
    _source : {enabled : 1},
    dynamic: "strict",
    properties: {
      name: {
        type: "string",
        index: "not_analyzed"
      },
      enabled: {
        type: "boolean"
      },
      lpValue: {
        type: "long"
      },
      lastRun: {
        type: "date"
      },
      count: {
        type: "long"
      },
      query: {
        type: "string",
        index: "not_analyzed"
      },
      action: {
        type: "string",
        index: "not_analyzed"
      },
      creator: {
        type: "string",
        index: "not_analyzed"
      },
      tags: {
        type: "string",
        index: "not_analyzed"
      }
    }
  }
}';

    print "Setting queries mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}queries/query/_mapping?pretty&ignore_conflicts=true", $mapping);
}

################################################################################
sub sessionsUpdate
{
    my $mapping = '
{
  session: {
    _all : {enabled : false},
    dynamic: "true",
    dynamic_templates: [
      {
        template_1: {
          path_match: "hdrs.*",
          match_mapping_type: "string",
          mapping: {
            type: "multi_field",
            path: "full",
            fields: {
              "snow" : {"type": "string", "analyzer" : "snowball"},
              "raw" : {"type": "string", "index" : "not_analyzed"}
            }
          }
        }
      }, {
        template_georir: {
          match_pattern: "regex",
          path_match: ".*-(geo|rir|term)$",
          match_mapping_type: "string",
          mapping: {
            omit_norms: true,
            type: "string",
            index: "not_analyzed"
          }
        }
      }, {
        template_string: {
          match_mapping_type: "string",
          mapping: {
            type: "multi_field",
            path: "full",
            fields: {
              "snow" : {"type": "string", "analyzer" : "snowball"},
              "raw" : {"type": "string", "index" : "not_analyzed"}
            }
          }
        }
      }
    ],
    properties: {
      us: {
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          us: {type: "string", analyzer: "url_analyzer"},
          rawus: {type: "string", index: "not_analyzed"}
        }
      },
      uscnt: {
        type: "integer"
      },
      ua: {
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          ua: {type: "string", analyzer: "snowball"},
          rawua: {type: "string", index: "not_analyzed"}
        }
      },
      uacnt: {
        type: "integer"
      },
      ps: {
        type: "long",
        index: "no"
      },
      psl: {
        type: "integer",
        index: "no"
      },
      fs: {
        type: "long"
      },
      lp: {
        type: "long",
        doc_values: true
      },
      lpd: {
        type: "date",
        doc_values: true
      },
      fp: {
        type: "long",
        doc_values: true
      },
      fpd: {
        type: "date",
        doc_values: true
      },
      a1: {
        type: "long",
        doc_values: true
      },
      g1: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      as1: {
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          as1: {type: "string", analyzer: "snowball"},
          rawas1: {type: "string", index: "not_analyzed"}
        }
      },
      rir1: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      p1: {
        type: "integer",
        doc_values: true
      },
      fb1: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      a2: {
        type: "long",
        doc_values: true
      },
      g2: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      as2: {
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          as2: {type: "string", analyzer: "snowball"},
          rawas2: {type: "string", index: "not_analyzed"}
        }
      },
      rir2: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      p2: {
        type: "integer",
        doc_values: true
      },
      fb2: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      xff: {
        type: "long"
      },
      xffcnt: {
        type: "integer"
      },
      xffscnt: {
        type: "integer"
      },
      gxff: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      asxff: {
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          asxff: {type: "string", analyzer: "snowball"},
          rawasxff: {type: "string", index: "not_analyzed"}
        }
      },
      rirxff: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      hmd5cnt: {
        type: "short"
      },
      hmd5 : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      dnshocnt: {
        type: "integer"
      },
      dnsho: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      dnsip: {
        type: "long"
      },
      dnsipcnt: {
        type: "integer"
      },
      gdnsip: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      asdnsip: {
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          asdnsip: {type: "string", analyzer: "snowball"},
          rawasdnsip: {type: "string", index: "not_analyzed"}
        }
      },
      rirdnsip: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      pr: {
        type: "short"
      },
      pa: {
        type: "integer"
      },
      pa1: {
        type: "integer"
      },
      pa2: {
        type: "integer"
      },
      by: {
        type: "long"
      },
      by1: {
        type: "long"
      },
      by2: {
        type: "long"
      },
      db: {
        type: "long"
      },
      db1: {
        type: "long"
      },
      db2: {
        type: "long"
      },
      ro: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      no: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      ho: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      hocnt: {
        type: "integer"
      },
      ta: {
        type: "integer"
      },
      tacnt: {
        type: "integer"
      },
      hh: {
        type: "integer"
      },
      hh1: {
        type: "integer"
      },
      hh2: {
        type: "integer"
      },
      hh1cnt: {
        type: "integer"
      },
      hh2cnt: {
        type: "integer"
      },
      hsver: {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      hsvercnt: {
        type: "integer"
      },
      hdver: {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      hdvercnt: {
        type: "integer"
      },
      hpath: {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      hpathcnt: {
        type: "integer"
      },
      hkey: {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      hkeycnt: {
        type: "integer"
      },
      hval: {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      hvalcnt: {
        type: "integer"
      },
      user: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      usercnt: {
        type: "integer"
      },
      tls : {
        type : "object",
        dynamic: "strict",
        properties : {
          iCn : {
            omit_norms: true,
            type : "string",
            index : "not_analyzed"
          },
          iOn : {
            type: "multi_field",
            omit_norms: true,
            fields: {
              "iOn": {type: "string", analyzer: "snowball"},
              "rawiOn": {type: "string", index: "not_analyzed"}
            }
          },
          sCn : {
            omit_norms: true,
            type : "string",
            index : "not_analyzed"
          },
          sOn : {
            type: "multi_field",
            omit_norms: true,
            fields: {
              "sOn": {type: "string", analyzer: "snowball"},
              "rawsOn": {type: "string", index: "not_analyzed"}
            }
          },
          sn : {
            omit_norms: true,
            type : "string",
            index : "not_analyzed"
          },
          alt : {
            omit_norms: true,
            type : "string",
            index : "not_analyzed"
          },
          altcnt: {
            type: "integer"
          },
          notBefore: {
            type: "long",
            index: "not_analyzed"
          },
          notAfter: {
            type: "long",
            index: "not_analyzed"
          },
          diffDays: {
            type: "integer",
            index: "not_analyzed"
          },
          hash : {
            omit_norms: true,
            type : "string",
            index : "not_analyzed"
          }
        }
      },
      tlscnt: {
        type: "integer"
      },
      sshkey : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      sshkeycnt: {
        type: "short"
      },
      sshver : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      sshvercnt: {
        type: "short"
      },
      euacnt: {
        type: "short"
      },
      eua : {
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          eua: {type: "string", analyzer: "snowball"},
          raweua: {type: "string", index: "not_analyzed"}
        }
      },
      esubcnt: {
        type: "short"
      },
      esub : {
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          esub: {type: "string", analyzer: "snowball"},
          rawesub: {type: "string", index: "not_analyzed"}
        }
      },
      eidcnt: {
        type: "short"
      },
      eid : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      ectcnt: {
        type: "short"
      },
      ect : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      emvcnt: {
        type: "short"
      },
      emv : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      efncnt: {
        type: "short"
      },
      efn : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      emd5cnt: {
        type: "short"
      },
      emd5 : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      esrccnt: {
        type: "short"
      },
      esrc : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      edstcnt: {
        type: "short"
      },
      edst : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      eho: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      ehocnt: {
        type: "integer"
      },
      eip: {
        type: "long"
      },
      eipcnt: {
        type: "integer"
      },
      ehh: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      ehhcnt: {
        type: "integer"
      },
      geip: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      aseip: {
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          aseip: {type: "string", analyzer: "snowball"},
          rawaseip: {type: "string", index: "not_analyzed"}
        }
      },
      rireip: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      ircnck: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      ircnckcnt: {
        type: "integer"
      },
      ircch: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      ircchcnt: {
        type: "integer"
      },
      hdrs: {
        type: "object",
        dynamic: "true"
      },
      plugin: {
        type: "object",
        dynamic: "true"
      },
      scrubat: {
        type: "date"
      },
      scrubby: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      smbdmcnt: {
        type: "short"
      },
      smbdm : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      smbfncnt: {
        type: "short"
      },
      smbfn : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      smbhocnt: {
        type: "short"
      },
      smbho : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      smboscnt: {
        type: "short"
      },
      smbos : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      smbshcnt: {
        type: "short"
      },
      smbsh : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      smbusercnt: {
        type: "short"
      },
      smbuser : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      smbvercnt: {
        type: "short"
      },
      smbver: {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      socksip: {
        type: "long"
      },
      gsocksip: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      assocksip: {
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          assocksip: {type: "string", analyzer: "snowball"},
          rawassocksip: {type: "string", index: "not_analyzed"}
        }
      },
      rirsocksip: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      sockspo: {
        type: "integer"
      },
      socksuser : {
        omit_norms: true,
        type : "string",
        index : "not_analyzed"
      },
      socksho: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      }
    }
  }
}
';

    my $template = '
{
  template: "' . $PREFIX . 'session*",
  settings: {
    index: {
      "routing.allocation.total_shards_per_node": 1,
      refresh_interval: 60,
      number_of_shards: ' . $main::numberOfNodes . ',
      number_of_replicas: 0,
      analysis: {
        analyzer : {
          url_analyzer : {
            type : "custom",
            tokenizer: "pattern",
            filter: ["lowercase"]
          }
        }
      }
    }
  },
  mappings:' . $mapping . '
}';

    print "Creating sessions template\n" if ($verbose > 0);
    #print "$template\n";
    esPut("/_template/${PREFIX}template_1", $template);

    my $indices = esGet("/${PREFIX}sessions-*/_aliases", 1);

    print "Updating sessions mapping for ", scalar(keys %{$indices}), " indices\n" if (scalar(keys %{$indices}) != 0);
    foreach my $i (keys %{$indices}) {
        progress($i);
        esPut("/$i/session/_mapping?ignore_conflicts=true", $mapping);

        # Before version 12 had soft, change to node, requires a close and open
        if ($main::versionNumber < 12) {
            esPost("/$i/_close", "");
            #esPut("/$i/_settings", '{"index.fielddata.cache": "node", "index.cache.field.type" : "node", "index.store.type": "mmapfs"}');
            esPut("/$i/_settings", '{"index.fielddata.cache": "node", "index.cache.field.type" : "node"}');
            esPost("/$i/_open", "");
        }
    }

    print "\n";

    esPut("/_cluster/settings", '{persistent: {"threadpool.search.queue_size":10000}}');
}

################################################################################
sub usersCreate
{
    my $settings = '
{
  settings: {
    number_of_shards: 1,
    number_of_replicas: 0,
    auto_expand_replicas: "0-2"
  }
}';

    print "Creating users_v3 index\n" if ($verbose > 0);
    esPut("/${PREFIX}users_v3", $settings);
    esAlias("add", "users_v3", "users");
    usersUpdate();
}
################################################################################
sub usersUpdate
{
    my $mapping = '
{
  user: {
    _all : {enabled : false},
    _source : {enabled : true},
    dynamic: "strict",
    properties: {
      userId: {
        type: "string",
        index: "not_analyzed"
      },
      userName: {
        type: "string",
        index: "not_analyzed"
      },
      enabled: {
        type: "boolean"
      },
      createEnabled: {
        type: "boolean"
      },
      webEnabled: {
        type: "boolean"
      },
      headerAuthEnabled: {
        type: "boolean"
      },
      emailSearch: {
        type: "boolean"
      },
      removeEnabled: {
        type: "boolean"
      },
      passStore: {
        type: "string",
        index: "not_analyzed"
      },
      expression: {
        type: "string",
        index: "not_analyzed"
      },
      settings : {
        type : "object",
        dynamic: "true"
      },
      views : {
        type : "object",
        dynamic: "true"
      }
    }
  }
}';

    print "Setting users_v3 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}users_v3/user/_mapping?pretty&ignore_conflicts=true", $mapping);
}

################################################################################
sub time2index
{
my($type, $t) = @_;

    my @t = gmtime($t);
    if ($type eq "hourly") {
        return sprintf("${PREFIX}sessions-%02d%02d%02dh%0d", $t[5] % 100, $t[4]+1, $t[3], $t[2]);
    } 

    if ($type eq "daily") {
        return sprintf("${PREFIX}sessions-%02d%02d%02d", $t[5] % 100, $t[4]+1, $t[3]);
    } 
    
    if ($type eq "weekly") {
        return sprintf("${PREFIX}sessions-%02dw%02d", $t[5] % 100, int($t[7]/7));
    } 
    
    if ($type eq "monthly") {
        return sprintf("${PREFIX}sessions-%02dm%02d", $t[5] % 100, $t[4]+1);
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
sub dbCheckHealth {
    my $health = esGet("/_cluster/health");
    if ($health->{status} ne "green") {
        print("WARNING elasticsearch health is '$health->{status}' instead of 'green', things may be broken\n\n");
    }

    my $settings = esGet("/_cluster/settings?flat_settings");
    if ($settings && $settings->{persistent} && $settings->{persistent}->{"threadpool.search.queue_size"} == "-1") {
        print "Changing threadpool.search.queue_size to 10000 to work around bug\n";
        esPut("/_cluster/settings", '{persistent: {"threadpool.search.queue_size":10000}}');
    }
}
################################################################################
sub dbCheck {
    my $esversion = dbESVersion();
    my @parts = split(/\./, $esversion->{version}->{number});
    $main::esVersion = int($parts[0]*100*100) + int($parts[1]*100) + int($parts[2]);

    if ($main::esVersion < 10200) {
        print("Currently using Elasticsearch version ", $esversion->{version}->{number}, " which isn't supported\n",
              "* 1.2.x is supported\n",
              "* 1.3.x is recommended\n",
              "\n",
              "Instructions: https://github.com/aol/moloch/wiki/FAQ#wiki-How_do_I_upgrade_Elastic_Search\n",
              "Make sure to restart any viewer or capture after upgrading!\n"
             );
        exit (1);

    }

    my $error = 0;
    my $nodes = esGet("/_nodes?settings&process&flat_settings");
    foreach my $key (sort {$nodes->{nodes}->{$a}->{name} cmp $nodes->{nodes}->{$b}->{name}} keys %{$nodes->{nodes}}) {
        next if (exists $nodes->{$key}->{attributes} && exists $nodes->{$key}->{attributes}->{data} && $nodes->{$key}->{attributes}->{data} eq "false");
        my $node = $nodes->{nodes}->{$key};
        my $errstr;
        my $warnstr;

        if (exists $node->{settings}->{"index.cache.field.type"}) {
            $errstr .= sprintf ("    REMOVE 'index.cache.field.type'\n");
        }

        if ($main::esVersion < 10400) {
            if (!(exists $node->{settings}->{"index.fielddata.cache"})) {
                $errstr .= sprintf ("       ADD 'index.fielddata.cache: node'\n");
            } elsif ($node->{settings}->{"index.fielddata.cache"} ne  "node") {
                $errstr .= sprintf ("    CHANGE 'index.fielddata.cache' to 'node'\n");
            }

            if (!(exists $node->{settings}->{"indices.fielddata.cache.size"})) {
                $errstr .= sprintf ("       ADD 'indices.fielddata.cache.size: 40%'\n");
            }
        }

        if ($main::esVersion < 10200 && !(exists $node->{settings}->{"script.disable_dynamic"})) {
            $warnstr .= sprintf ("       ADD 'script.disable_dynamic: true'\n");
            $warnstr .= sprintf ("         - Closes a potential security issue\n");
        }

        if (!(exists $node->{process}->{max_file_descriptors}) || int($node->{process}->{max_file_descriptors}) < 4000) {
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
sub progress {
    my ($index) = @_;
    if ($verbose == 1) {
        local $| = 1;
        print ".";
    } elsif ($verbose == 2) {
        local $| = 1;
        print "$index ";
    }
}
################################################################################
sub optimizeOther {
    print "Optimizing Admin Indices\n";
    foreach my $i ("${PREFIX}dstats_v1", "${PREFIX}files_v3", "${PREFIX}sequence", "${PREFIX}tags_v2", "${PREFIX}users_v3") {
        progress($i);
        esGet("/$i/_optimize?max_num_segments=1", 1);
        if ($main::esVersion >= 10400) {
            esGet("/$i/_upgrade", 1);
        }
    }
    print "\n";
    print "\n" if ($verbose > 0);
}
################################################################################
while (@ARGV > 0 && substr($ARGV[0], 0, 1) eq "-") {
    if ($ARGV[0] =~ /(-v+|--verbose)$/) {
         $verbose += ($ARGV[0] =~ tr/v//);
    } elsif ($ARGV[0] =~ /--prefix$/) {
        $PREFIX = $ARGV[1];
        shift @ARGV;
        $PREFIX .= "_" if ($PREFIX !~ /_$/);
    } else {
        showHelp("Unknkown option $ARGV[0]")
    }
    shift @ARGV;
}

showHelp("Help:") if ($ARGV[1] =~ /^help$/);
showHelp("Missing arguments") if (@ARGV < 2);
showHelp("Unknown command '$ARGV[1]'") if ($ARGV[1] !~ /^(init|initnoprompt|info|wipe|upgrade|users-?import|users-?export|expire|rotate|optimize|mv|rm|rm-?missing|rm-?node|field)$/);
showHelp("Missing arguments") if (@ARGV < 3 && $ARGV[1] =~ /^(users-?import|users-?export|rm|rm-?missing|rm-?node)$/);
showHelp("Missing arguments") if (@ARGV < 4 && $ARGV[1] =~ /^(field)$/);
showHelp("Must have both <old fn> and <new fn>") if (@ARGV < 4 && $ARGV[1] =~ /^(mv)$/);
showHelp("Must have both <type> and <num> arguments") if (@ARGV < 4 && $ARGV[1] =~ /^(rotate|expire)$/);

$main::userAgent = LWP::UserAgent->new(timeout => 20);

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
    my $indices = esGet("/${PREFIX}sessions-*/_aliases", 1);

    my $endTime = time();
    my $endTimeIndex = time2index($ARGV[2], $endTime);
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

    my $optimizecnt = 0;
    my $startTime = mktime(@startTime);
    while ($startTime <= $endTime) {
        my $iname = time2index($ARGV[2], $startTime);
        if (exists $indices->{$iname}) {
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
    $main::userAgent->timeout(900);
    optimizeOther();
    printf ("Expiring %s indices, optimizing %s\n", commify(scalar(keys %{$indices}) - $optimizecnt), commify($optimizecnt));
    foreach my $i (sort (keys %{$indices})) {
        progress($i);
        if (exists $indices->{$i}->{OPTIMIZEIT}) {
            esPut("/$i/_settings?index.codec.bloom.load=false", "", 1);
            esGet("/$i/_optimize?max_num_segments=4", 1);
        } else {
            esDelete("/$i", 1);
        }
    }
    exit 0;
} elsif ($ARGV[1] eq "optimize") {
    my $indices = esGet("/${PREFIX}sessions-*/_aliases", 1);

    dbESVersion();
    $main::userAgent->timeout(600);
    optimizeOther();
    printf "Optimizing %s Session Indices\n", commify(scalar(keys %{$indices}));
    foreach my $i (sort (keys %{$indices})) {
        progress($i);
        esGet("/$i/_optimize?max_num_segments=4", 1);
        if ($main::esVersion >= 10400) {
            esGet("/$i/_upgrade", 1);
        }
    }
    print "\n";
    exit 0;
} elsif ($ARGV[1] eq "info") {
    dbVersion(0);
    my $esversion = dbESVersion();
    my $nodes = esGet("/_nodes");
    my $status = esGet("/_status", 1);
    my $sessions = 0;
    my $sessionsBytes = 0;
    my @sessions = grep /^${PREFIX}session/, keys %{$status->{indices}};
    foreach my $index (@sessions) {
        next if ($index !~ /^${PREFIX}sessions-/);
        $sessions += $status->{indices}->{$index}->{docs}->{num_docs};
        $sessionsBytes += $status->{indices}->{$index}->{index}->{primary_size_in_bytes};
    }

sub printIndex {
    my ($index, $name) = @_;
    return if (!$index);
    printf "%-20s %s (%s bytes)\n", $name . ":", commify($index->{docs}->{num_docs}), commify($index->{index}->{primary_size_in_bytes});
}

    printf "ES Version:          %s\n", $esversion->{version}->{number};
    printf "DB Version:          %s\n", $main::versionNumber;
    printf "ES Nodes:            %s/%s\n", commify(dataNodes($nodes->{nodes})), commify(scalar(keys %{$nodes->{nodes}}));
    printf "Session Indices:     %s\n", commify(scalar(@sessions));
    printf "Sessions:            %s (%s bytes)\n", commify($sessions), commify($sessionsBytes);
    if (scalar(@sessions) > 0) {
        printf "Session Density:     %s (%s bytes)\n", commify(int($sessions/(scalar(keys %{$nodes->{nodes}})*scalar(@sessions)))), 
                                                       commify(int($sessionsBytes/(scalar(keys %{$nodes->{nodes}})*scalar(@sessions))));
    }
    printIndex($status->{indices}->{files_v3}, "files_v3");
    printIndex($status->{indices}->{files_v2}, "files_v2");
    printIndex($status->{indices}->{files_v1}, "files_v1");
    printIndex($status->{indices}->{tags_v2}, "tags_v2");
    printIndex($status->{indices}->{tags_v1}, "tags_v1");
    printIndex($status->{indices}->{users_v3}, "users_v3");
    printIndex($status->{indices}->{users_v2}, "users_v2");
    printIndex($status->{indices}->{users_v1}, "users_v1");
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
    my $results = esGet("/${PREFIX}files/_search?size=50000&q=node:$ARGV[2]");
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
    my $results = esGet("/${PREFIX}files/_search?size=50000&q=node:$ARGV[2]");
    print "Deleting ", $results->{hits}->{total}, " files\n";
    foreach my $hit (@{$results->{hits}->{hits}}) {
        esDelete("/${PREFIX}files/file/" . $hit->{_id}, 0);
    }
    esDelete("/${PREFIX}stats/stat/" . $ARGV[2], 1);
    my $results = esGet("/${PREFIX}dstats/_search?size=50000&q=nodeName:$ARGV[2]");
    print "Deleting ", $results->{hits}->{total}, " stats\n";
    foreach my $hit (@{$results->{hits}->{hits}}) {
        esDelete("/${PREFIX}dstats/dstat/" . $hit->{_id}, 0);
    }
    exit 0;
} elsif ($ARGV[1] =~ /^(field)$/) {
    my $result = esGet("/${PREFIX}fields/field/$ARGV[3]", 1);
    my $found = $result->{found};
    die "Field $ARGV[3] isn't found" if (!$found);

    esPost("/${PREFIX}fields/field/$ARGV[3]/_update", "{\"doc\":{\"disabled\":" . ($ARGV[2] eq "disable"?"true":"false").  "}}");
    
    exit 0;
}

sub dataNodes
{
my ($nodes) = @_;
    my $total = 0;

    foreach my $key (keys %{$nodes}) {
        next if (exists $nodes->{$key}->{attributes} && exists $nodes->{$key}->{attributes}->{data} && $nodes->{$key}->{attributes}->{data} eq "false");
        $total++;
    }
    return $total;
}


dbCheckHealth();

my $nodes = esGet("/_nodes");
$main::numberOfNodes = dataNodes($nodes->{nodes});
print "It is STRONGLY recommended that you stop ALL moloch captures and viewers before proceeding.\n\n";
if ($main::numberOfNodes == 1) {
    print "There is $main::numberOfNodes elastic search data node, if you expect more please fix first before proceeding.\n\n";
} else {
    print "There are $main::numberOfNodes elastic search data nodes, if you expect more please fix first before proceeding.\n\n";
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
    esDelete("/${PREFIX}tags_v2", 1);
    esDelete("/${PREFIX}tags", 1);
    esDelete("/${PREFIX}sequence", 1);
    esDelete("/${PREFIX}files_v3", 1);
    esDelete("/${PREFIX}files_v2", 1);
    esDelete("/${PREFIX}files_v1", 1);
    esDelete("/${PREFIX}files", 1);
    esDelete("/${PREFIX}stats", 1);
    esDelete("/${PREFIX}dstats", 1);
    esDelete("/${PREFIX}fields", 1);
    esDelete("/${PREFIX}dstats_v1", 1);
    esDelete("/${PREFIX}sessions*", 1);
    esDelete("/${PREFIX}template_1", 1);
    if ($ARGV[1] =~ "init") {
        esDelete("/${PREFIX}users_v1", 1);
        esDelete("/${PREFIX}users_v2", 1);
        esDelete("/${PREFIX}users_v3", 1);
        esDelete("/${PREFIX}users", 1);
        esDelete("/${PREFIX}queries", 1);
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
    if ($ARGV[1] =~ "init") {
        usersCreate();
        queriesCreate();
    }
    print "Finished.  Have fun!\n";
} elsif ($main::versionNumber == 0) {
    print "Trying to upgrade from version 0 to version $VERSION.  This may or may not work since the elasticsearch moloch db was a wildwest before version 1.  This upgrade will reset some of the stats, sorry.\n\n";
    waitFor("UPGRADE", "do you want to upgrade?");
    print "Starting Upgrade\n";

    esDelete("/${PREFIX}stats", 1);

    tagsUpdate();
    sequenceUpdate();
    statsCreate();
    sessionsUpdate();
    fieldsCreate();

    filesCreate();
    esAlias("remove", "files_v1", "files");
    esCopy("files_v1", "files_v3", "file");

    usersCreate();
    esAlias("remove", "users_v1", "users");
    esCopy("users_v1", "users_v3", "user");

    dstatsCreate();
    esCopy("dstats", "dstats_v1", "user");
    sleep 1;

    esDelete("/${PREFIX}dstats", 1);
    sleep 1;

    esAlias("add", "dstats_v1", "dstats");

    queriesCreate();

    print "users_v1 and files_v1 tables can be deleted now\n";
    print "Finished\n";
} elsif ($main::versionNumber >= 1 && $main::versionNumber < 7) {
    print "Trying to upgrade from version $main::versionNumber to version $VERSION.\n\n";
    waitFor("UPGRADE", "do you want to upgrade?");
    print "Starting Upgrade\n";

    filesCreate();
    esAlias("remove", "files_v2", "files");
    esCopy("files_v2", "files_v3", "file");

    usersCreate();
    esAlias("remove", "users_v2", "users");
    esCopy("users_v2", "users_v3", "user");
    print "users_v2 and files_v2 table can be deleted now\n";

    sessionsUpdate();
    statsUpdate();
    dstatsUpdate();
    fieldsCreate();
    queriesCreate();

    print "Finished\n";
} elsif ($main::versionNumber >= 7 && $main::versionNumber < 18) {
    print "Trying to upgrade from version $main::versionNumber to version $VERSION.\n\n";
    waitFor("UPGRADE", "do you want to upgrade?");
    print "Starting Upgrade\n";

    usersCreate();
    esAlias("remove", "users_v2", "users");
    esCopy("users_v2", "users_v3", "user");
    print "users_v2 table can be deleted now\n";

    filesUpdate();
    sessionsUpdate();
    statsUpdate();
    dstatsUpdate();
    fieldsCreate();
    queriesCreate();

    print "Finished\n";
} elsif ($main::versionNumber >= 18 && $main::versionNumber < 19) {
    print "Trying to upgrade from version $main::versionNumber to version $VERSION.\n\n";
    waitFor("UPGRADE", "do you want to upgrade?");
    print "Starting Upgrade\n";

    usersCreate();
    esAlias("remove", "users_v2", "users");
    esCopy("users_v2", "users_v3", "user");
    print "users_v2 table can be deleted now\n";

    fieldsUpdate();
    sessionsUpdate();
    queriesCreate();
    statsUpdate();
    dstatsUpdate();

    print "Finished\n";
} elsif ($main::versionNumber >= 19 && $main::versionNumber < 20) {
    print "Trying to upgrade from version $main::versionNumber to version $VERSION.\n\n";
    waitFor("UPGRADE", "do you want to upgrade?");
    sessionsUpdate();
    queriesCreate();
    fieldsUpdate();
    statsUpdate();
    dstatsUpdate();

    print "Finished\n";
} elsif ($main::versionNumber >= 20 && $main::versionNumber <= 25) {
    print "Trying to upgrade from version $main::versionNumber to version $VERSION.\n\n";
    waitFor("UPGRADE", "do you want to upgrade?");
    sessionsUpdate();
    fieldsUpdate();
    statsUpdate();
    dstatsUpdate();

    print "Finished\n";
} else {
    print "db.pl is hosed\n";
}

sleep 1;
esPost("/${PREFIX}dstats/version/version", "{\"version\": $VERSION}");
