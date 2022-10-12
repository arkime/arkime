#!/usr/bin/perl
# This script can initialize, upgrade or provide simple maintenance for the
# Arkime elastic search db
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
# 61 - shortcuts
# 62 - hunt error timestamp and node
# 63 - Upgrade for ES 7.x: sequence_v3, fields_v3, queries_v3, files_v6, users_v7, dstats_v4, stats_v4, hunts_v2
# 64 - lock shortcuts
# 65 - hunt unrunnable and failedSessionIds
# 66 - share hunts
# 67 - remove hunt info from matched sessions
# 68 - cron query enhancements
# 70 - reindex everything, ecs, sessions3
# 71 - user.roles, user.cont3xt
# 72 - save es query in history, hunt description
# 73 - hunt roles
# 74 - shortcut sharing with users/roles
# 75 - notifiers index
# 76 - views index
# 77 - cron sharing with roles and users
# 78 - added roleAssigners to users

use HTTP::Request::Common;
use LWP::UserAgent;
use JSON;
use MIME::Base64;
use Data::Dumper;
use POSIX;
use IO::Compress::Gzip qw(gzip $GzipError);
use IO::Uncompress::Gunzip qw(gunzip $GunzipError);
use strict;
no if ($] >= 5.018), 'warnings' => 'experimental';

my $VERSION = 78;
my $verbose = 0;
my $PREFIX = undef;
my $OLDPREFIX = "";
my $SECURE = 1;
my $CLIENTCERT = "";
my $CLIENTKEY = "";
my $NOCHANGES = 0;
my $SHARDS = -1;
my $REPLICAS = -1;
my $HISTORY = 13;
my $SEGMENTS = 1;
my $SEGMENTSMIN = -1;
my $NOOPTIMIZE = 0;
my $FULL = 0;
my $REVERSE = 0;
my $SHARDSPERNODE = 1;
my $ESTIMEOUT=60;
my $UPGRADEALLSESSIONS = 1;
my $DOHOTWARM = 0;
my $DOILM = 0;
my $WARMAFTER = -1;
my $WARMKIND = "daily";
my $OPTIMIZEWARM = 0;
my $TYPE = "string";
my $SHAREROLES = "";
my $SHAREUSERS = "";
my $DESCRIPTION = "";
my $LOCKED = 0;
my $GZ = 0;
my $REFRESH = 60;
my $ESAPIKEY = "";
my $USERPASS;

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
    print "  --clientkey <keypath>        - Path to key for client authentication.  Must not have a passphrase.\n";
    print "  --clientcert <certpath>      - Path to cert for client authentication\n";
    print "  --insecure                   - Disable certificate verification for https calls\n";
    print "  -n                           - Make no db changes\n";
    print "  --timeout <timeout>          - Timeout in seconds for ES, default 60\n";
    print "  --esuser <user>[:<password>] - ES User and Password\n";
    print "  --esapikey <key>             - Same key as elasticsearchAPIKey in your Arkime config file\n";
    print "\n";
    print "General Commands:\n";
    print "  info                         - Information about the database\n";
    print "  repair                       - Try and repair a corrupted cluster schema\n";
    print "  init [<opts>]                - Clear ALL elasticsearch Arkime data and create schema\n";
    print "    --shards <shards>          - Number of shards for sessions, default number of nodes\n";
    print "    --replicas <num>           - Number of replicas for sessions, default 0\n";
    print "    --refresh <num>            - Number of seconds for ES refresh interval for sessions indices, default 60\n";
    print "    --shardsPerNode <shards>   - Number of shards per node or use \"null\" to let ES decide, default shards*replicas/nodes\n";
    print "    --hotwarm                  - Set 'hot' for 'node.attr.molochtype' on new indices, warm on non sessions indices\n";
    print "    --ilm                      - Use ilm to manage\n";
    print "  wipe                         - Same as init, but leaves user database untouched\n";
    print "  upgrade [<opts>]             - Upgrade Arkime's schema in elasticsearch from previous versions\n";
    print "    --shards <shards>          - Number of shards for sessions, default number of nodes\n";
    print "    --replicas <num>           - Number of replicas for sessions, default 0\n";
    print "    --refresh <num>            - Number of seconds for ES refresh interval for sessions indices, default 60\n";
    print "    --shardsPerNode <shards>   - Number of shards per node or use \"null\" to let ES decide, default shards*replicas/nodes\n";
    print "    --hotwarm                  - Set 'hot' for 'node.attr.molochtype' on new indices, warm on non sessions indices\n";
    print "    --ilm                      - Use ilm to manage\n";
    print "  expire <type> <num> [<opts>] - Perform daily ES maintenance and optimize all indices in ES\n";
    print "       type                    - Same as rotateIndex in ini file = hourly,hourlyN,daily,weekly,monthly\n";
    print "       num                     - Number of indexes to keep\n";
    print "    --replicas <num>           - Number of replicas for older sessions indices, default 0\n";
    print "    --nooptimize               - Do not optimize session indexes during this operation\n";
    print "    --history <num>            - Number of weeks of history to keep, default 13\n";
    print "    --segments <num>           - Number of segments to optimize sessions to, default 1\n";
    print "    --segmentsmin <num>        - Only optimize indices with at least <num> segments, default is <segments> \n";
    print "    --reverse                  - Optimize from most recent to oldest\n";
    print "    --shardsPerNode <shards>   - Number of shards per node or use \"null\" to let ES decide, default shards*replicas/nodes\n";
    print "    --warmafter <wafter>       - Set molochwarm on indices after <wafter> <type>\n";
    print "    --optmizewarm              - Only optimize warm green indices\n";
    print "  optimize                     - Optimize all Arkime indices in ES\n";
    print "    --segments <num>           - Number of segments to optimize sessions to, default 1\n";
    print "  optimize-admin               - Optimize only admin indices in ES, use with ILM\n";
    print "  disable-users <days>         - Disable user accounts that have not been active\n";
    print "      days                     - Number of days of inactivity (integer)\n";
    print "  set-shortcut <name> <userid> <file> [<opts>]\n";
    print "       name                    - Name of the shortcut (no special characters except '_')\n";
    print "       userid                  - UserId of the user to add the shortcut for\n";
    print "       file                    - File that includes a comma or newline separated list of values\n";
    print "    --type <type>              - Type of shortcut = string, ip, number, default is string\n";
    print "    --shareRoles               - Share to roles (comma separated list of roles)\n";
    print "    --shareUsers               - Share to specific users (comma seprated list of userIds)\n";
    print "    --description <description>- Description of the shortcut\n";
    print "    --locked                   - Whether the shortcut is locked and cannot be modified by the web interface\n";
    print "  shrink <index> <node> <num>  - Shrink a session index\n";
    print "      index                    - The session index to shrink\n";
    print "      node                     - The node to temporarily use for shrinking\n";
    print "      num                      - Number of shards to shrink to\n";
    print "    --shardsPerNode <shards>   - Number of shards per node or use \"null\" to let ES decide, default 1\n";
    print "  ilm <force> <delete>         - Create ILM profile\n";
    print "      force                    - Time in hours/days before (moving to warm) and force merge (number followed by h or d)\n";
    print "      delete                   - Time in hours/days before deleting index (number followed by h or d)\n";
    print "    --hotwarm                  - Set 'hot' for 'node.attr.molochtype' on new indices, warm on non sessions indices\n";
    print "    --segments <num>           - Number of segments to optimize sessions to, default 1\n";
    print "    --replicas <num>           - Number of replicas for older sessions indices, default 0\n";
    print "    --history <num>            - Number of weeks of history to keep, default 13\n";
    print "  reindex <src> [<dst>]        - Reindex ES indices\n";
    print "    --nopcap                   - Remove fields having to do with pcap files\n";
    print "\n";
    print "Backup and Restore Commands:\n";
    print "  backup <basename> <opts>     - Backup everything but sessions/history; filenames created start with <basename>\n";
    print "    --gz                       - GZip the files\n";
    print "  restore <basename> [<opts>]  - Restore everything but sessions/history; filenames restored from start with <basename>\n";
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
    print "  field disable <exp>          - Disable a field from being indexed\n";
    print "  field enable <exp>           - Enable a field from being indexed\n";
    print "\n";
    print "Node Commands:\n";
    print "  rm-node <node>               - Remove from db all data for node (doesn't change disk)\n";
    print "  add-alias <node> <hostname>  - Adds a hidden node that points to hostname\n";
    print "  hide-node <node>             - Hide node in stats display\n";
    print "  unhide-node <node>           - Unhide node in stats display\n";
    print "\n";
    print "ES maintenance\n";
    print "  set-replicas <pat> <num>              - Set the number of replicas for index pattern\n";
    print "  set-shards-per-node <pat> <num>       - Set the number of shards per node for index pattern\n";
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
sub waitForRE
{
    my ($re, $help) = @_;

    print "$help\n";
    while (1) {
        my $answer = <STDIN>;
        chomp $answer;
        return $answer if ($answer =~ $re);
        print "$help\n";
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
    if (($response->code == 500 && $ARGV[1] ne "init" && $ARGV[1] ne "shrink") || ($response->code != 200 && !$dontcheck)) {
      die "Couldn't GET ${main::elasticsearch}$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?";
    }
    my $json = from_json($response->content);
    logmsg "GET RESULT:", Dumper($json), "\n" if ($verbose > 3 || $response->code == 401);
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

      logmsg "POST RESULT:", $response->content, "\n" if ($response->code == 401 || $verbose > 3 || ($verbose > 0 && int($response->code / 100) == 4));
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
    if ($response->code != 200 && !$dontcheck) {
      logmsg Dumper($response);
      die "Couldn't PUT ${main::elasticsearch}$url  the http status code is " . $response->code . " are you sure elasticsearch is running/reachable?\n" . $response->content;
    } elsif ($response->code == 500 && $dontcheck) {
      print "Ignoring following error\n";
      logmsg Dumper($response);
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
    logmsg "Copying " . $status->{indices}->{$srci}->{primaries}->{docs}->{count} . " elements from $srci to $dsti\n";

    esPost("/_reindex?timeout=7200s", to_json({"source" => {"index" => $srci}, "dest" => {"index" => $dsti, "version_type" => "external"}, "conflicts" => "proceed"}));

    my $status = esGet("/${dsti}/_refresh", 1);
    my $status = esGet("/_stats/docs", 1);
    if ($status->{indices}->{$srci}->{primaries}->{docs}->{count} > $status->{indices}->{$dsti}->{primaries}->{docs}->{count}) {
        logmsg $status->{indices}->{$srci}->{primaries}->{docs}->{count}, " > ",  $status->{indices}->{$dsti}->{primaries}->{docs}->{count}, "\n";
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
        esPost("/_aliases?master_timeout=${ESTIMEOUT}s", '{ "actions": [ { "' . $cmd . '": { "index": "' . $PREFIX . $index . '", "alias" : "'. $PREFIX . $alias .'" } } ] }', 1);
    } else { # do not append PREFIX
        esPost("/_aliases?master_timeout=${ESTIMEOUT}s", '{ "actions": [ { "' . $cmd . '": { "index": "' . $index . '", "alias" : "'. $alias .'" } } ] }', 1);
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
    my ($index, $segments, $dowait) = @_;
    esWaitForNoTask("forcemerge") if ($dowait);
    esPost("/$index/_forcemerge?max_num_segments=$segments", "", 2);
    esWaitForNoTask("forcemerge") if ($dowait);
}
################################################################################
sub esMatchingIndices
{
    my $indices = esGet("/_cat/indices/$_[0]?format=json", 1);
    return "" if (ref ($indices) ne "ARRAY");
    my %indices = map { $_->{index} => $_ } @{$indices};
    return join(",", keys (%indices));
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

    logmsg "Creating sequence_v30 index\n" if ($verbose > 0);
    esPut("/${PREFIX}sequence_v30?master_timeout=${ESTIMEOUT}s", $settings, 1);
    esAlias("add", "sequence_v30", "sequence");
    sequenceUpdate();
}

################################################################################
sub sequenceUpdate
{
    my $mapping = '
{
  "_source" : { "enabled": "false" },
  "enabled" : "false"
}';

    logmsg "Setting sequence_v30 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}sequence_v30/_mapping?master_timeout=${ESTIMEOUT}s", $mapping);
}
################################################################################
sub sequenceUpgrade
{

    if (esCheckAlias("${PREFIX}sequence", "${PREFIX}sequence_v30") && esIndexExists("${PREFIX}sequence_v30")) {
        logmsg ("SKIPPING - ${PREFIX}sequence already points to ${PREFIX}sequence_v30\n");
        return;
    }

    $main::userAgent->timeout(7200);
    sequenceCreate();
    esAlias("remove", "sequence_v3", "sequence");
    my $results = esGet("/${OLDPREFIX}sequence_v3/_search?version=true&size=10000&rest_total_hits_as_int=true", 0);

    logmsg "Copying " . $results->{hits}->{total} . " elements from ${OLDPREFIX}sequence_v3 to ${PREFIX}sequence_v30\n";

    return if ($results->{hits}->{total} == 0);

    foreach my $hit (@{$results->{hits}->{hits}}) {
        if ($hit->{_id} =~ /^fn-/) {
            esPost("/${PREFIX}sequence_v30/_doc/$hit->{_id}?timeout=${ESTIMEOUT}s&version_type=external&version=$hit->{_version}", "{}", 1);
        }
    }
    esDelete("/${OLDPREFIX}sequence_v3");
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

    logmsg "Creating files_v30 index\n" if ($verbose > 0);
    esPut("/${PREFIX}files_v30?master_timeout=${ESTIMEOUT}s", $settings);
    esAlias("add", "files_v30", "files");
    filesUpdate();
}
################################################################################
sub filesUpdate
{
    my $mapping = '
{
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
}';

    logmsg "Setting files_v30 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}files_v30/_mapping?master_timeout=${ESTIMEOUT}s", $mapping);
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
    esPut("/${PREFIX}stats_v30?master_timeout=${ESTIMEOUT}s", $settings);
    esAlias("add", "stats_v30", "stats");
    statsUpdate();
}

################################################################################
sub statsUpdate
{
my $mapping = '
{
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
}';

    logmsg "Setting stats mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}stats_v30/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping, 1);
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

    logmsg "Creating dstats_v30 index\n" if ($verbose > 0);
    esPut("/${PREFIX}dstats_v30?master_timeout=${ESTIMEOUT}s", $settings);
    esAlias("add", "dstats_v30", "dstats");
    dstatsUpdate();
}

################################################################################
sub dstatsUpdate
{
my $mapping = '
{
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
}';

    logmsg "Setting dstats_v30 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}dstats_v30/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping, 1);
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
    esPut("/${PREFIX}fields_v30?master_timeout=${ESTIMEOUT}s", $settings);
    esAlias("add", "fields_v30", "fields");
    fieldsUpdate();
}
################################################################################
sub newField
{
    my ($field, $json) = @_;
    esPost("/${PREFIX}fields_v30/_doc/$field?timeout=${ESTIMEOUT}s", $json);
}
################################################################################
sub fieldsUpdate
{
    my $mapping = '
{
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
}';

    logmsg "Setting fields_v30 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}fields_v30/_mapping?master_timeout=${ESTIMEOUT}s", $mapping);

    esPost("/${PREFIX}fields_v30/_doc/ip?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "All IP fields",
      "group": "general",
      "help": "Search all ip fields",
      "type": "ip",
      "dbField2": "ipall",
      "portField": "portall",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v30/_doc/port?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "All port fields",
      "group": "general",
      "help": "Search all port fields",
      "type": "integer",
      "dbField2": "portall",
      "regex": "(^port\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.port$)"
    }');
    esPost("/${PREFIX}fields_v30/_doc/rir?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "All rir fields",
      "group": "general",
      "help": "Search all rir fields",
      "type": "uptermfield",
      "dbField2": "rirall",
      "regex": "(^rir\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.rir$)"
    }');
    esPost("/${PREFIX}fields_v30/_doc/country?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "All country fields",
      "group": "general",
      "help": "Search all country fields",
      "type": "uptermfield",
      "dbField2": "geoall",
      "regex": "(^country\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.country$)"
    }');
    esPost("/${PREFIX}fields_v30/_doc/asn?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "All ASN fields",
      "group": "general",
      "help": "Search all ASN fields",
      "type": "termfield",
      "dbField2": "asnall",
      "regex": "(^asn\\\\.(?:(?!\\\\.cnt$).)*$|\\\\.asn$)"
    }');
    esPost("/${PREFIX}fields_v30/_doc/host?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "All Host fields",
      "group": "general",
      "help": "Search all Host fields",
      "type": "lotermfield",
      "dbField2": "hostall",
      "regex": "(^host\\\\.(?:(?!\\\\.(cnt|tokens)$).)*$|\\\\.host$)"
    }');
    esPost("/${PREFIX}fields_v30/_doc/ip.src?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Src IP",
      "group": "general",
      "help": "Source IP",
      "type": "ip",
      "dbField2": "srcIp",
      "portField": "p1",
      "portField2": "srcPort",
      "portFieldECS": "source.port",
      "category": "ip"
    }');
    esPost("/${PREFIX}fields_v30/_doc/port.src?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Src Port",
      "group": "general",
      "help": "Source Port",
      "type": "integer",
      "dbField2": "srcPort",
      "category": "port"
    }');
    esPost("/${PREFIX}fields_v30/_doc/asn.src?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Src ASN",
      "group": "general",
      "help": "GeoIP ASN string calculated from the source IP",
      "type": "termfield",
      "dbField2": "srcASN",
      "category": "asn"
    }');
    esPost("/${PREFIX}fields_v30/_doc/asn.src?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Src ASN",
      "group": "general",
      "help": "GeoIP ASN string calculated from the source IP",
      "type": "termfield",
      "dbField2": "srcASN",
      "category": "asn"
    }');
    newField("source.as.number", '{
      "friendlyName": "Src ASN Number",
      "group": "general",
      "help": "GeoIP ASN Number calculated from the source IP",
      "type": "integer",
      "fieldECS": "source.as.number"
    }');
    newField("source.as.organization.name", '{
      "friendlyName": "Src ASN Name",
      "group": "general",
      "help": "GeoIP ASN Name calculated from the source IP",
      "type": "termfield",
      "fieldECS": "source.as.organization.name"
    }');
    esPost("/${PREFIX}fields_v30/_doc/country.src?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Src Country",
      "group": "general",
      "help": "Source Country",
      "type": "uptermfield",
      "dbField2": "srcGEO",
      "category": "country"
    }');
    esPost("/${PREFIX}fields_v30/_doc/rir.src?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Src RIR",
      "group": "general",
      "help": "Source RIR",
      "type": "uptermfield",
      "dbField2": "srcRIR",
      "category": "rir"
    }');
    esPost("/${PREFIX}fields_v30/_doc/ip.dst?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Dst IP",
      "group": "general",
      "help": "Destination IP",
      "type": "ip",
      "dbField2": "dstIp",
      "portField2": "dstPort",
      "portFieldECS": "destination.port",
      "category": "ip",
      "aliases": ["ip.dst:port"]
    }');
    esPost("/${PREFIX}fields_v30/_doc/port.dst?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Dst Port",
      "group": "general",
      "help": "Source Port",
      "type": "integer",
      "dbField2": "dstPort",
      "category": "port"
    }');
    esPost("/${PREFIX}fields_v30/_doc/asn.dst?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Dst ASN",
      "group": "general",
      "help": "GeoIP ASN string calculated from the destination IP",
      "type": "termfield",
      "dbField2": "dstASN",
      "category": "asn"
    }');
    newField("destination.as.number", '{
      "friendlyName": "Dst ASN Number",
      "group": "general",
      "help": "GeoIP ASN Number calculated from the destination IP",
      "type": "integer",
      "fieldECS": "destination.as.number"
    }');
    newField("destination.as.organization.name", '{
      "friendlyName": "Dst ASN Name",
      "group": "general",
      "help": "GeoIP ASN Name calculated from the destination IP",
      "type": "termfield",
      "fieldECS": "destination.as.organization.name"
    }');
    esPost("/${PREFIX}fields_v30/_doc/country.dst?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Dst Country",
      "group": "general",
      "help": "Destination Country",
      "type": "uptermfield",
      "dbField2": "dstGEO",
      "category": "country"
    }');
    esPost("/${PREFIX}fields_v30/_doc/rir.dst?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Dst RIR",
      "group": "general",
      "help": "Destination RIR",
      "type": "uptermfield",
      "dbField2": "dstRIR",
      "category": "rir"
    }');
    esPost("/${PREFIX}fields_v30/_doc/bytes?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent AND received in a session",
      "type": "integer",
      "dbField2": "totBytes"
    }');
    esPost("/${PREFIX}fields_v30/_doc/bytes.src?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Src Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent by source in a session",
      "type": "integer",
      "dbField2": "srcBytes"
    }');
    esPost("/${PREFIX}fields_v30/_doc/bytes.dst?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Dst Bytes",
      "group": "general",
      "help": "Total number of raw bytes sent by destination in a session",
      "type": "integer",
      "dbField2": "dstBytes"
    }');
    esPost("/${PREFIX}fields_v30/_doc/databytes?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Data bytes",
      "group": "general",
      "help": "Total number of data bytes sent AND received in a session",
      "type": "integer",
      "dbField2": "totDataBytes"
    }');
    esPost("/${PREFIX}fields_v30/_doc/databytes.src?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Src data bytes",
      "group": "general",
      "help": "Total number of data bytes sent by source in a session",
      "type": "integer",
      "dbField2": "srcDataBytes"
    }');
    esPost("/${PREFIX}fields_v30/_doc/databytes.dst?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Dst data bytes",
      "group": "general",
      "help": "Total number of data bytes sent by destination in a session",
      "type": "integer",
      "dbField2": "dstDataBytes"
    }');
    esPost("/${PREFIX}fields_v30/_doc/packets?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Packets",
      "group": "general",
      "help": "Total number of packets sent AND received in a session",
      "type": "integer",
      "dbField2": "totPackets"
    }');
    esPost("/${PREFIX}fields_v30/_doc/packets.src?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Src Packets",
      "group": "general",
      "help": "Total number of packets sent by source in a session",
      "type": "integer",
      "dbField2": "srcPackets"
    }');
    esPost("/${PREFIX}fields_v30/_doc/packets.dst?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Dst Packets",
      "group": "general",
      "help": "Total number of packets sent by destination in a session",
      "type": "integer",
      "dbField2": "dstPackets"
    }');
    esPost("/${PREFIX}fields_v30/_doc/ip.protocol?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "IP Protocol",
      "group": "general",
      "help": "IP protocol number or friendly name",
      "type": "lotermfield",
      "dbField2": "ipProtocol",
      "transform": "ipProtocolLookup"
    }');
    esPost("/${PREFIX}fields_v30/_doc/id?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Arkime ID",
      "group": "general",
      "help": "Arkime ID for the session",
      "type": "termfield",
      "dbField2": "_id",
      "noFacet": "true"

    }');
    esPost("/${PREFIX}fields_v30/_doc/rootId?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Arkime Root ID",
      "group": "general",
      "help": "Arkime ID of the first session in a multi session stream",
      "type": "termfield",
      "dbField2": "rootId"
    }');
    esPost("/${PREFIX}fields_v30/_doc/node?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Arkime Node",
      "group": "general",
      "help": "Arkime node name the session was recorded on",
      "type": "termfield",
      "dbField2": "node"
    }');
    esPost("/${PREFIX}fields_v30/_doc/srcNode?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Arkime Source Node",
      "group": "general",
      "help": "Source Arkime node name the session was recorded on when using send to cluster",
      "type": "termfield",
      "dbField2": "srcNode"
    }');
    esPost("/${PREFIX}fields_v30/_doc/file?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Filename",
      "group": "general",
      "help": "Arkime offline pcap filename",
      "type": "fileand",
      "dbField2": "fileand"
    }');
    esPost("/${PREFIX}fields_v30/_doc/payload8.src.hex?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Payload Src Hex",
      "group": "general",
      "help": "First 8 bytes of source payload in hex",
      "type": "lotermfield",
      "dbField2": "srcPayload8",
      "aliases": ["payload.src"]
    }');
    esPost("/${PREFIX}fields_v30/_doc/payload8.src.utf8?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Payload Src UTF8",
      "group": "general",
      "help": "First 8 bytes of source payload in utf8",
      "type": "termfield",
      "dbField2": "srcPayload8",
      "transform": "utf8ToHex",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v30/_doc/payload8.dst.hex?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Payload Dst Hex",
      "group": "general",
      "help": "First 8 bytes of destination payload in hex",
      "type": "lotermfield",
      "dbField2": "dstPayload8",
      "aliases": ["payload.dst"]
    }');
    esPost("/${PREFIX}fields_v30/_doc/payload8.dst.utf8?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Payload Dst UTF8",
      "group": "general",
      "help": "First 8 bytes of destination payload in utf8",
      "type": "termfield",
      "dbField2": "dstPayload8",
      "transform": "utf8ToHex",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v30/_doc/payload8.hex?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Payload Hex",
      "group": "general",
      "help": "First 8 bytes of payload in hex",
      "type": "lotermfield",
      "dbField2": "fballhex",
      "regex": "^payload8.(src|dst).hex$"
    }');
    esPost("/${PREFIX}fields_v30/_doc/payload8.utf8?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Payload UTF8",
      "group": "general",
      "help": "First 8 bytes of payload in hex",
      "type": "lotermfield",
      "dbField2": "fballutf8",
      "regex": "^payload8.(src|dst).utf8$"
    }');
    esPost("/${PREFIX}fields_v30/_doc/scrubbed.by?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Scrubbed By",
      "group": "general",
      "help": "SPI data was scrubbed by",
      "type": "lotermfield",
      "dbField2": "scrubby"
    }');
    esPost("/${PREFIX}fields_v30/_doc/view?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "View Name",
      "group": "general",
      "help": "Arkime view name",
      "type": "viewand",
      "dbField2": "viewand",
      "noFacet": "true"
    }');
    esPost("/${PREFIX}fields_v30/_doc/starttime?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Start Time",
      "group": "general",
      "help": "Session Start Time",
      "type": "seconds",
      "type2": "date",
      "dbField2": "firstPacket"
    }');
    esPost("/${PREFIX}fields_v30/_doc/stoptime?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Stop Time",
      "group": "general",
      "help": "Session Stop Time",
      "type": "seconds",
      "type2": "date",
      "dbField2": "lastPacket"
    }');
    esPost("/${PREFIX}fields_v30/_doc/huntId?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Hunt ID",
      "group": "general",
      "help": "The ID of the packet search job that matched this session",
      "type": "termfield",
      "dbField2": "huntId"
    }');
    esPost("/${PREFIX}fields_v30/_doc/huntName?timeout=${ESTIMEOUT}s", '{
      "friendlyName": "Hunt Name",
      "group": "general",
      "help": "The name of the packet search job that matched this session",
      "type": "termfield",
      "dbField2": "huntName"
    }');
    ecsFieldsUpdate();
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
    esPut("/${PREFIX}queries_v30?master_timeout=${ESTIMEOUT}s", $settings);
    queriesUpdate();
}
################################################################################
sub queriesUpdate
{
    my $mapping = '
{
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
    "lastCount": {
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
    },
    "description": {
      "type": "keyword"
    },
    "created": {
      "type": "date"
    },
    "lastToggled": {
      "type": "date"
    },
    "lastToggledBy": {
      "type": "keyword"
    },
    "roles": {
      "type": "keyword"
    },
    "users": {
      "type": "keyword"
    }
  }
}';

    logmsg "Setting queries mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}queries_v30/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping);
    esAlias("add", "queries_v30", "queries");
}

################################################################################
my %ECSMAP;
my %ECSPROP;

sub addECSMap
{
  my ($exp, $db, $fieldECS) = @_;

  $ECSMAP{$exp}->{fieldECS} = $fieldECS if ($exp ne 'null');
  $ECSPROP{$fieldECS}->{path} = $db;
  $ECSPROP{$fieldECS}->{type} = "alias";
}

addECSMap("country.dst", "dstGEO", "destination.geo.country_iso_code");
addECSMap("asn.dst", "dstASN", "destination.as.full");
addECSMap("bytes.dst", "dstBytes", "destination.bytes");
addECSMap("databytes.dst", "dstDataBytes", "server.bytes");
addECSMap("packets.dst", "dstPackets", "destination.packets");
addECSMap("ip.dst", "dstIp", "destination.ip");
addECSMap("port.dst", "dstPort", "destination.port");
addECSMap("mac.dst", "dstMac", "destination.mac");
addECSMap("null", "dstMacCnt", "destination.mac-cnt");

addECSMap("country.src", "srcGEO", "source.geo.country_iso_code");
addECSMap("asn.src", "srcASN", "source.as.full");
addECSMap("bytes.src", "srcBytes", "source.bytes");
addECSMap("databytes.src", "srcDataBytes", "client.bytes");
addECSMap("packets.src", "srcPackets", "source.packets");
addECSMap("ip.src", "srcIp", "source.ip");
addECSMap("port.src", "srcPort", "source.port");
addECSMap("mac.src", "srcMac", "source.mac");
addECSMap("null", "srcMacCnt", "source.mac-cnt");


addECSMap("communityId", "communityId", "network.community_id");
addECSMap("bytes", "totBytes", "network.bytes");
addECSMap("packets", "totPackets", "network.packets");
addECSMap("vlan", "vlan", "network.vlan.id");
addECSMap('null', "vlanCnt", "network.vlan.id-cnt");

addECSMap('null', "timestamp", "\@timestamp");
################################################################################

sub ecsFieldsUpdate
{
    foreach my $key (keys (%ECSMAP)) {
        esPost("/${PREFIX}fields/_update/$key", qq({"doc":{"fieldECS": "$ECSMAP{$key}->{fieldECS}"}}), 1);
    }

    #print '{"properties":' . to_json(\%ECSPROP) . "}\n";
    foreach my $key (keys (%ECSPROP)) {
        esPut("/${OLDPREFIX}sessions2-*/_mapping", qq({"properties": {"$key":) .  to_json($ECSPROP{$key}) . qq(}}), 1);
    }
}

################################################################################

sub sessions3ECSTemplate
{
# Modfified version of https://raw.githubusercontent.com/elastic/ecs/1.10/generated/elasticsearch/7/template.json
# 1) change index_patterns
# 2) Delete cloud,dns,http,tls,user,data_stream
# 3) Add source.as.full, destination.as.full, source.mac-cnt, destination.mac-cnt, network.vlan.id-cnt
my $template = '
{
  "index_patterns": "' . $PREFIX . 'sessions3-*",
  "mappings": {
    "_meta": {
      "version": "1.10.0"
    },
    "date_detection": false,
    "dynamic_templates": [
      {
        "strings_as_keyword": {
          "mapping": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "match_mapping_type": "string"
        }
      }
    ],
    "properties": {
      "@timestamp": {
        "type": "date"
      },
      "agent": {
        "properties": {
          "build": {
            "properties": {
              "original": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "ephemeral_id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "type": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "version": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "client": {
        "properties": {
          "address": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "as": {
            "properties": {
              "number": {
                "type": "long"
              },
              "organization": {
                "properties": {
                  "name": {
                    "fields": {
                      "text": {
                        "norms": false,
                        "type": "text"
                      }
                    },
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              }
            }
          },
          "bytes": {
            "type": "long"
          },
          "domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "geo": {
            "properties": {
              "city_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "location": {
                "type": "geo_point"
              },
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "postal_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "timezone": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "ip": {
            "type": "ip"
          },
          "mac": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "nat": {
            "properties": {
              "ip": {
                "type": "ip"
              },
              "port": {
                "type": "long"
              }
            }
          },
          "packets": {
            "type": "long"
          },
          "port": {
            "type": "long"
          },
          "registered_domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "subdomain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "top_level_domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "user": {
            "properties": {
              "domain": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "email": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "full_name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "group": {
                "properties": {
                  "domain": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "hash": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "roles": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          }
        }
      },
      "container": {
        "properties": {
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "image": {
            "properties": {
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "tag": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "labels": {
            "type": "object"
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "runtime": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "destination": {
        "properties": {
          "address": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "as": {
            "properties": {
              "full" : {
                "type" : "keyword"
              },
              "number": {
                "type": "long"
              },
              "organization": {
                "properties": {
                  "name": {
                    "fields": {
                      "text": {
                        "norms": false,
                        "type": "text"
                      }
                    },
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              }
            }
          },
          "bytes": {
            "type": "long"
          },
          "domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "geo": {
            "properties": {
              "city_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "location": {
                "type": "geo_point"
              },
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "postal_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "timezone": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "ip": {
            "type": "ip"
          },
          "mac": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "mac-cnt" : {
            "type" : "long"
          },
          "nat": {
            "properties": {
              "ip": {
                "type": "ip"
              },
              "port": {
                "type": "long"
              }
            }
          },
          "packets": {
            "type": "long"
          },
          "port": {
            "type": "long"
          },
          "registered_domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "subdomain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "top_level_domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "user": {
            "properties": {
              "domain": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "email": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "full_name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "group": {
                "properties": {
                  "domain": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "hash": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "roles": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          }
        }
      },
      "dll": {
        "properties": {
          "code_signature": {
            "properties": {
              "exists": {
                "type": "boolean"
              },
              "signing_id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "status": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "subject_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "team_id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "trusted": {
                "type": "boolean"
              },
              "valid": {
                "type": "boolean"
              }
            }
          },
          "hash": {
            "properties": {
              "md5": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "sha1": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "sha256": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "sha512": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "ssdeep": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "path": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "pe": {
            "properties": {
              "architecture": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "company": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "description": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "file_version": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "imphash": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "original_file_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "product": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          }
        }
      },
      "ecs": {
        "properties": {
          "version": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "error": {
        "properties": {
          "code": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "message": {
            "norms": false,
            "type": "text"
          },
          "stack_trace": {
            "doc_values": false,
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "index": false,
            "type": "keyword"
          },
          "type": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "event": {
        "properties": {
          "action": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "category": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "code": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "created": {
            "type": "date"
          },
          "dataset": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "duration": {
            "type": "long"
          },
          "end": {
            "type": "date"
          },
          "hash": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "ingested": {
            "type": "date"
          },
          "kind": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "module": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "original": {
            "doc_values": false,
            "ignore_above": 1024,
            "index": false,
            "type": "keyword"
          },
          "outcome": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "provider": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "reason": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "reference": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "risk_score": {
            "type": "float"
          },
          "risk_score_norm": {
            "type": "float"
          },
          "sequence": {
            "type": "long"
          },
          "severity": {
            "type": "long"
          },
          "start": {
            "type": "date"
          },
          "timezone": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "type": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "url": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "file": {
        "properties": {
          "accessed": {
            "type": "date"
          },
          "attributes": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "code_signature": {
            "properties": {
              "exists": {
                "type": "boolean"
              },
              "signing_id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "status": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "subject_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "team_id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "trusted": {
                "type": "boolean"
              },
              "valid": {
                "type": "boolean"
              }
            }
          },
          "created": {
            "type": "date"
          },
          "ctime": {
            "type": "date"
          },
          "device": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "directory": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "drive_letter": {
            "ignore_above": 1,
            "type": "keyword"
          },
          "extension": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "gid": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "group": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "hash": {
            "properties": {
              "md5": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "sha1": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "sha256": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "sha512": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "ssdeep": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "inode": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "mime_type": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "mode": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "mtime": {
            "type": "date"
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "owner": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "path": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          },
          "pe": {
            "properties": {
              "architecture": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "company": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "description": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "file_version": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "imphash": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "original_file_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "product": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "size": {
            "type": "long"
          },
          "target_path": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          },
          "type": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "uid": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "x509": {
            "properties": {
              "alternative_names": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "issuer": {
                "properties": {
                  "common_name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "country": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "distinguished_name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "locality": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "organization": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "organizational_unit": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "state_or_province": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "not_after": {
                "type": "date"
              },
              "not_before": {
                "type": "date"
              },
              "public_key_algorithm": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "public_key_curve": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "public_key_exponent": {
                "doc_values": false,
                "index": false,
                "type": "long"
              },
              "public_key_size": {
                "type": "long"
              },
              "serial_number": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "signature_algorithm": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "subject": {
                "properties": {
                  "common_name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "country": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "distinguished_name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "locality": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "organization": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "organizational_unit": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "state_or_province": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "version_number": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          }
        }
      },
      "group": {
        "properties": {
          "domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "host": {
        "properties": {
          "architecture": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "cpu": {
            "properties": {
              "usage": {
                "scaling_factor": 1000,
                "type": "scaled_float"
              }
            }
          },
          "disk": {
            "properties": {
              "read": {
                "properties": {
                  "bytes": {
                    "type": "long"
                  }
                }
              },
              "write": {
                "properties": {
                  "bytes": {
                    "type": "long"
                  }
                }
              }
            }
          },
          "domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "geo": {
            "properties": {
              "city_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "location": {
                "type": "geo_point"
              },
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "postal_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "timezone": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "hostname": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "ip": {
            "type": "ip"
          },
          "mac": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "network": {
            "properties": {
              "egress": {
                "properties": {
                  "bytes": {
                    "type": "long"
                  },
                  "packets": {
                    "type": "long"
                  }
                }
              },
              "ingress": {
                "properties": {
                  "bytes": {
                    "type": "long"
                  },
                  "packets": {
                    "type": "long"
                  }
                }
              }
            }
          },
          "os": {
            "properties": {
              "family": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "full": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "kernel": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "platform": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "type": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "version": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "type": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "uptime": {
            "type": "long"
          },
          "user": {
            "properties": {
              "domain": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "email": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "full_name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "group": {
                "properties": {
                  "domain": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "hash": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "roles": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          }
        }
      },
      "labels": {
        "type": "object"
      },
      "log": {
        "properties": {
          "file": {
            "properties": {
              "path": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "level": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "logger": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "origin": {
            "properties": {
              "file": {
                "properties": {
                  "line": {
                    "type": "integer"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "function": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "original": {
            "doc_values": false,
            "ignore_above": 1024,
            "index": false,
            "type": "keyword"
          },
          "syslog": {
            "properties": {
              "facility": {
                "properties": {
                  "code": {
                    "type": "long"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "priority": {
                "type": "long"
              },
              "severity": {
                "properties": {
                  "code": {
                    "type": "long"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              }
            },
            "type": "object"
          }
        }
      },
      "message": {
        "norms": false,
        "type": "text"
      },
      "network": {
        "properties": {
          "application": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "bytes": {
            "type": "long"
          },
          "community_id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "direction": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "forwarded_ip": {
            "type": "ip"
          },
          "iana_number": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "inner": {
            "properties": {
              "vlan": {
                "properties": {
                  "id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              }
            },
            "type": "object"
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "packets": {
            "type": "long"
          },
          "protocol": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "transport": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "type": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "vlan": {
            "properties": {
              "id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "id-cnt" : {
                "type" : "long"
              },
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          }
        }
      },
      "observer": {
        "properties": {
          "egress": {
            "properties": {
              "interface": {
                "properties": {
                  "alias": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "vlan": {
                "properties": {
                  "id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "zone": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            },
            "type": "object"
          },
          "geo": {
            "properties": {
              "city_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "location": {
                "type": "geo_point"
              },
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "postal_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "timezone": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "hostname": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "ingress": {
            "properties": {
              "interface": {
                "properties": {
                  "alias": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "vlan": {
                "properties": {
                  "id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "zone": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            },
            "type": "object"
          },
          "ip": {
            "type": "ip"
          },
          "mac": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "os": {
            "properties": {
              "family": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "full": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "kernel": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "platform": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "type": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "version": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "product": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "serial_number": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "type": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "vendor": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "version": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "orchestrator": {
        "properties": {
          "api_version": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "cluster": {
            "properties": {
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "url": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "version": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "namespace": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "organization": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "resource": {
            "properties": {
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "type": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "type": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "organization": {
        "properties": {
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "name": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "package": {
        "properties": {
          "architecture": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "build_version": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "checksum": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "description": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "install_scope": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "installed": {
            "type": "date"
          },
          "license": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "path": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "reference": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "size": {
            "type": "long"
          },
          "type": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "version": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "process": {
        "properties": {
          "args": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "args_count": {
            "type": "long"
          },
          "code_signature": {
            "properties": {
              "exists": {
                "type": "boolean"
              },
              "signing_id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "status": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "subject_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "team_id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "trusted": {
                "type": "boolean"
              },
              "valid": {
                "type": "boolean"
              }
            }
          },
          "command_line": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          },
          "entity_id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "executable": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          },
          "exit_code": {
            "type": "long"
          },
          "hash": {
            "properties": {
              "md5": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "sha1": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "sha256": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "sha512": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "ssdeep": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "name": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          },
          "parent": {
            "properties": {
              "args": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "args_count": {
                "type": "long"
              },
              "code_signature": {
                "properties": {
                  "exists": {
                    "type": "boolean"
                  },
                  "signing_id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "status": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "subject_name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "team_id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "trusted": {
                    "type": "boolean"
                  },
                  "valid": {
                    "type": "boolean"
                  }
                }
              },
              "command_line": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "entity_id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "executable": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "exit_code": {
                "type": "long"
              },
              "hash": {
                "properties": {
                  "md5": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "sha1": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "sha256": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "sha512": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "ssdeep": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "pe": {
                "properties": {
                  "architecture": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "company": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "description": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "file_version": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "imphash": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "original_file_name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "product": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "pgid": {
                "type": "long"
              },
              "pid": {
                "type": "long"
              },
              "ppid": {
                "type": "long"
              },
              "start": {
                "type": "date"
              },
              "thread": {
                "properties": {
                  "id": {
                    "type": "long"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "title": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "uptime": {
                "type": "long"
              },
              "working_directory": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "pe": {
            "properties": {
              "architecture": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "company": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "description": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "file_version": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "imphash": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "original_file_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "product": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "pgid": {
            "type": "long"
          },
          "pid": {
            "type": "long"
          },
          "ppid": {
            "type": "long"
          },
          "start": {
            "type": "date"
          },
          "thread": {
            "properties": {
              "id": {
                "type": "long"
              },
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "title": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          },
          "uptime": {
            "type": "long"
          },
          "working_directory": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "registry": {
        "properties": {
          "data": {
            "properties": {
              "bytes": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "strings": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "type": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "hive": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "key": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "path": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "value": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "related": {
        "properties": {
          "hash": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "hosts": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "ip": {
            "type": "ip"
          },
          "user": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "rule": {
        "properties": {
          "author": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "category": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "description": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "license": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "reference": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "ruleset": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "uuid": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "version": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "server": {
        "properties": {
          "address": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "as": {
            "properties": {
              "number": {
                "type": "long"
              },
              "organization": {
                "properties": {
                  "name": {
                    "fields": {
                      "text": {
                        "norms": false,
                        "type": "text"
                      }
                    },
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              }
            }
          },
          "bytes": {
            "type": "long"
          },
          "domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "geo": {
            "properties": {
              "city_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "location": {
                "type": "geo_point"
              },
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "postal_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "timezone": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "ip": {
            "type": "ip"
          },
          "mac": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "nat": {
            "properties": {
              "ip": {
                "type": "ip"
              },
              "port": {
                "type": "long"
              }
            }
          },
          "packets": {
            "type": "long"
          },
          "port": {
            "type": "long"
          },
          "registered_domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "subdomain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "top_level_domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "user": {
            "properties": {
              "domain": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "email": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "full_name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "group": {
                "properties": {
                  "domain": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "hash": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "roles": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          }
        }
      },
      "service": {
        "properties": {
          "ephemeral_id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "node": {
            "properties": {
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "state": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "type": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "version": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "source": {
        "properties": {
          "address": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "as": {
            "properties": {
              "full" : {
                "type" : "keyword"
              },
              "number": {
                "type": "long"
              },
              "organization": {
                "properties": {
                  "name": {
                    "fields": {
                      "text": {
                        "norms": false,
                        "type": "text"
                      }
                    },
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              }
            }
          },
          "bytes": {
            "type": "long"
          },
          "domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "geo": {
            "properties": {
              "city_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "continent_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "country_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "location": {
                "type": "geo_point"
              },
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "postal_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_iso_code": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "region_name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "timezone": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "ip": {
            "type": "ip"
          },
          "mac": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "mac-cnt" : {
            "type" : "long"
          },
          "nat": {
            "properties": {
              "ip": {
                "type": "ip"
              },
              "port": {
                "type": "long"
              }
            }
          },
          "packets": {
            "type": "long"
          },
          "port": {
            "type": "long"
          },
          "registered_domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "subdomain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "top_level_domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "user": {
            "properties": {
              "domain": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "email": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "full_name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "group": {
                "properties": {
                  "domain": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "name": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              },
              "hash": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "roles": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          }
        }
      },
      "span": {
        "properties": {
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "tags": {
        "ignore_above": 1024,
        "type": "keyword"
      },
      "threat": {
        "properties": {
          "framework": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "tactic": {
            "properties": {
              "id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "reference": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "technique": {
            "properties": {
              "id": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "reference": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "subtechnique": {
                "properties": {
                  "id": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "name": {
                    "fields": {
                      "text": {
                        "norms": false,
                        "type": "text"
                      }
                    },
                    "ignore_above": 1024,
                    "type": "keyword"
                  },
                  "reference": {
                    "ignore_above": 1024,
                    "type": "keyword"
                  }
                }
              }
            }
          }
        }
      },
      "trace": {
        "properties": {
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "transaction": {
        "properties": {
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "url": {
        "properties": {
          "domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "extension": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "fragment": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "full": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          },
          "original": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          },
          "password": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "path": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "port": {
            "type": "long"
          },
          "query": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "registered_domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "scheme": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "subdomain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "top_level_domain": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "username": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "user_agent": {
        "properties": {
          "device": {
            "properties": {
              "name": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "name": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "original": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          },
          "os": {
            "properties": {
              "family": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "full": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "kernel": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "name": {
                "fields": {
                  "text": {
                    "norms": false,
                    "type": "text"
                  }
                },
                "ignore_above": 1024,
                "type": "keyword"
              },
              "platform": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "type": {
                "ignore_above": 1024,
                "type": "keyword"
              },
              "version": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "version": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      },
      "vulnerability": {
        "properties": {
          "category": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "classification": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "description": {
            "fields": {
              "text": {
                "norms": false,
                "type": "text"
              }
            },
            "ignore_above": 1024,
            "type": "keyword"
          },
          "enumeration": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "reference": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "report_id": {
            "ignore_above": 1024,
            "type": "keyword"
          },
          "scanner": {
            "properties": {
              "vendor": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "score": {
            "properties": {
              "base": {
                "type": "float"
              },
              "environmental": {
                "type": "float"
              },
              "temporal": {
                "type": "float"
              },
              "version": {
                "ignore_above": 1024,
                "type": "keyword"
              }
            }
          },
          "severity": {
            "ignore_above": 1024,
            "type": "keyword"
          }
        }
      }
    }
  },
  "order": 1,
  "settings": {
    "index": {
      "mapping": {
        "total_fields": {
          "limit": 10000
        }
      },
      "refresh_interval": "5s"
    }
  }
}
';
    logmsg "Creating sessions ecs template\n" if ($verbose > 0);
    esPut("/_template/${PREFIX}sessions3_ecs_template?master_timeout=${ESTIMEOUT}s&pretty", $template);
}
################################################################################
# Create the template sessions use and update mapping of current sessions.
# Not all fields need to be here, but the index will be created quicker if more are.
sub sessions3Update
{
    my $mapping = '
{
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
  "properties" : {
    "asset" : {
      "type" : "keyword"
    },
    "assetCnt" : {
      "type" : "long"
    },
    "bgp" : {
      "properties" : {
        "type" : {
          "type" : "keyword"
        }
      }
    },
    "cert" : {
      "properties" : {
        "alt" : {
          "type" : "keyword"
        },
        "altCnt" : {
          "type" : "long"
        },
        "curve" : {
          "type" : "keyword"
        },
        "hash" : {
          "type" : "keyword"
        },
        "issuerCN" : {
          "type" : "keyword"
        },
        "issuerON" : {
          "type" : "keyword"
        },
        "notAfter" : {
          "type" : "date"
        },
        "notBefore" : {
          "type" : "date"
        },
        "publicAlgorithm" : {
          "type" : "keyword"
        },
        "remainingDays" : {
          "type" : "long"
        },
        "serial" : {
          "type" : "keyword"
        },
        "subjectCN" : {
          "type" : "keyword"
        },
        "subjectON" : {
          "type" : "keyword"
        },
        "validDays" : {
          "type" : "long"
        }
      }
    },
    "certCnt" : {
      "type" : "long"
    },
    "dhcp" : {
      "properties" : {
        "host" : {
          "type" : "keyword",
          "copy_to" : [
            "dhcp.hostTokens"
          ]
        },
        "hostCnt" : {
          "type" : "long"
        },
        "hostTokens" : {
          "type" : "text",
          "norms" : false,
          "analyzer" : "wordSplit"
        },
        "id" : {
          "type" : "keyword"
        },
        "idCnt" : {
          "type" : "long"
        },
        "mac" : {
          "type" : "keyword"
        },
        "macCnt" : {
          "type" : "long"
        },
        "oui" : {
          "type" : "keyword"
        },
        "ouiCnt" : {
          "type" : "long"
        },
        "type" : {
          "type" : "keyword"
        },
        "typeCnt" : {
          "type" : "long"
        }
      }
    },
    "dns" : {
      "properties" : {
        "ASN" : {
          "type" : "keyword"
        },
        "GEO" : {
          "type" : "keyword"
        },
        "RIR" : {
          "type" : "keyword"
        },
        "host" : {
          "type" : "keyword",
          "copy_to" : [
            "dns.hostTokens"
          ]
        },
        "hostCnt" : {
          "type" : "long"
        },
        "hostTokens" : {
          "type" : "text",
          "norms" : false,
          "analyzer" : "wordSplit"
        },
        "ip" : {
          "type" : "ip"
        },
        "ipCnt" : {
          "type" : "long"
        },
        "opcode" : {
          "type" : "keyword"
        },
        "opcodeCnt" : {
          "type" : "long"
        },
        "puny" : {
          "type" : "keyword"
        },
        "punyCnt" : {
          "type" : "long"
        },
        "qc" : {
          "type" : "keyword"
        },
        "qcCnt" : {
          "type" : "long"
        },
        "qt" : {
          "type" : "keyword"
        },
        "qtCnt" : {
          "type" : "long"
        },
        "status" : {
          "type" : "keyword"
        },
        "statusCnt" : {
          "type" : "long"
        }
      }
    },
    "dstOui" : {
      "type" : "keyword"
    },
    "dstOuiCnt" : {
      "type" : "long"
    },
    "dstPayload8" : {
      "type" : "keyword"
    },
    "dstRIR" : {
      "type" : "keyword"
    },
    "email" : {
      "properties" : {
        "ASN" : {
          "type" : "keyword"
        },
        "GEO" : {
          "type" : "keyword"
        },
        "RIR" : {
          "type" : "keyword"
        },
        "bodyMagic" : {
          "type" : "keyword"
        },
        "bodyMagicCnt" : {
          "type" : "long"
        },
        "contentType" : {
          "type" : "keyword"
        },
        "contentTypeCnt" : {
          "type" : "long"
        },
        "dst" : {
          "type" : "keyword"
        },
        "dstCnt" : {
          "type" : "long"
        },
        "filename" : {
          "type" : "keyword"
        },
        "filenameCnt" : {
          "type" : "long"
        },
        "header" : {
          "type" : "keyword"
        },
        "header-chad" : {
          "type" : "keyword"
        },
        "header-chadCnt" : {
          "type" : "long"
        },
        "headerCnt" : {
          "type" : "long"
        },
        "host" : {
          "type" : "keyword",
          "copy_to" : [
            "email.hostTokens"
          ]
        },
        "hostCnt" : {
          "type" : "long"
        },
        "hostTokens" : {
          "type" : "text",
          "norms" : false,
          "analyzer" : "wordSplit"
        },
        "id" : {
          "type" : "keyword"
        },
        "idCnt" : {
          "type" : "long"
        },
        "ip" : {
          "type" : "ip"
        },
        "ipCnt" : {
          "type" : "long"
        },
        "md5" : {
          "type" : "keyword"
        },
        "md5Cnt" : {
          "type" : "long"
        },
        "mimeVersion" : {
          "type" : "keyword"
        },
        "mimeVersionCnt" : {
          "type" : "long"
        },
        "smtpHello" : {
          "type" : "keyword"
        },
        "smtpHelloCnt" : {
          "type" : "long"
        },
        "src" : {
          "type" : "keyword"
        },
        "srcCnt" : {
          "type" : "long"
        },
        "subject" : {
          "type" : "keyword"
        },
        "subjectCnt" : {
          "type" : "long"
        },
        "useragent" : {
          "type" : "keyword"
        },
        "useragentCnt" : {
          "type" : "long"
        }
      }
    },
    "fileId" : {
      "type" : "long"
    },
    "firstPacket" : {
      "type" : "date"
    },
    "srcOuterIp" : {
      "type" : "ip"
    },
    "srcOuterIpCnt" : {
      "type" : "long"
    },
    "dstOuterIp" : {
      "type" : "ip"
    },
    "dstOuterIpCnt" : {
      "type" : "long"
    },
    "srcOuterOui" : {
      "type" : "keyword"
    },
    "srcOuterOuiCnt" : {
      "type" : "long"
    },
    "dstOuterOui" : {
      "type" : "keyword"
    },
    "dstOuterOuiCnt" : {
      "type" : "long"
    },
    "srcOuterMac" : {
      "type" : "keyword"
    },
    "srcOuterMacCnt" : {
      "type" : "long"
    },
    "dstOuterMac" : {
      "type" : "keyword"
    },
    "dstOuterMacCnt" : {
      "type" : "long"
    },
    "srcOuterRIR" : {
      "type" : "keyword"
    },
    "dstOuterRIR" : {
      "type" : "keyword"
    },
    "srcOuterGEO" : {
      "type" : "keyword"
    },
    "dstOuterGEO" : {
      "type" : "keyword"
    },
    "srcOuterASN" : {
      "type" : "keyword"
    },
    "dstOuterASN" : {
      "type" : "keyword"
    },
    "http" : {
      "properties" : {
        "authType" : {
          "type" : "keyword"
        },
        "authTypeCnt" : {
          "type" : "long"
        },
        "bodyMagic" : {
          "type" : "keyword"
        },
        "bodyMagicCnt" : {
          "type" : "long"
        },
        "clientVersion" : {
          "type" : "keyword"
        },
        "clientVersionCnt" : {
          "type" : "long"
        },
        "cookieKey" : {
          "type" : "keyword"
        },
        "cookieKeyCnt" : {
          "type" : "long"
        },
        "cookieValue" : {
          "type" : "keyword"
        },
        "cookieValueCnt" : {
          "type" : "long"
        },
        "host" : {
          "type" : "keyword",
          "copy_to" : [
            "http.hostTokens"
          ]
        },
        "hostCnt" : {
          "type" : "long"
        },
        "hostTokens" : {
          "type" : "text",
          "norms" : false,
          "analyzer" : "wordSplit"
        },
        "key" : {
          "type" : "keyword"
        },
        "keyCnt" : {
          "type" : "long"
        },
        "md5" : {
          "type" : "keyword"
        },
        "md5Cnt" : {
          "type" : "long"
        },
        "method" : {
          "type" : "keyword"
        },
        "methodCnt" : {
          "type" : "long"
        },
        "path" : {
          "type" : "keyword"
        },
        "pathCnt" : {
          "type" : "long"
        },
        "request-authorization" : {
          "type" : "keyword"
        },
        "request-authorizationCnt" : {
          "type" : "long"
        },
        "request-chad" : {
          "type" : "keyword"
        },
        "request-chadCnt" : {
          "type" : "long"
        },
        "request-content-type" : {
          "type" : "keyword"
        },
        "request-content-typeCnt" : {
          "type" : "long"
        },
        "request-origin" : {
          "type" : "keyword"
        },
        "request-referer" : {
          "type" : "keyword"
        },
        "request-refererCnt" : {
          "type" : "long"
        },
        "requestBody" : {
          "type" : "keyword"
        },
        "requestHeader" : {
          "type" : "keyword"
        },
        "requestHeaderCnt" : {
          "type" : "long"
        },
        "response-content-type" : {
          "type" : "keyword"
        },
        "response-content-typeCnt" : {
          "type" : "long"
        },
        "response-location" : {
          "type" : "keyword"
        },
        "response-server" : {
          "type" : "keyword"
        },
        "responseHeader" : {
          "type" : "keyword"
        },
        "responseHeaderCnt" : {
          "type" : "long"
        },
        "serverVersion" : {
          "type" : "keyword"
        },
        "serverVersionCnt" : {
          "type" : "long"
        },
        "statuscode" : {
          "type" : "long"
        },
        "statuscodeCnt" : {
          "type" : "long"
        },
        "uri" : {
          "type" : "keyword",
          "copy_to" : [
            "http.uriTokens"
          ]
        },
        "uriCnt" : {
          "type" : "long"
        },
        "uriTokens" : {
          "type" : "text",
          "norms" : false,
          "analyzer" : "wordSplit"
        },
        "user" : {
          "type" : "keyword"
        },
        "userCnt" : {
          "type" : "long"
        },
        "useragent" : {
          "type" : "keyword",
          "copy_to" : [
            "http.useragentTokens"
          ]
        },
        "useragentCnt" : {
          "type" : "long"
        },
        "useragentTokens" : {
          "type" : "text",
          "norms" : false,
          "analyzer" : "wordSplit"
        },
        "value" : {
          "type" : "keyword"
        },
        "valueCnt" : {
          "type" : "long"
        },
        "xffASN" : {
          "type" : "keyword"
        },
        "xffGEO" : {
          "type" : "keyword"
        },
        "xffIp" : {
          "type" : "ip"
        },
        "xffIpCnt" : {
          "type" : "long"
        },
        "xffRIR" : {
          "type" : "keyword"
        }
      }
    },
    "icmp" : {
      "properties" : {
        "code" : {
          "type" : "long"
        },
        "type" : {
          "type" : "long"
        }
      }
    },
    "initRTT" : {
      "type" : "long"
    },
    "ipProtocol" : {
      "type" : "long"
    },
    "irc" : {
      "properties" : {
        "channel" : {
          "type" : "keyword"
        },
        "channelCnt" : {
          "type" : "long"
        },
        "nick" : {
          "type" : "keyword"
        },
        "nickCnt" : {
          "type" : "long"
        }
      }
    },
    "krb5" : {
      "properties" : {
        "cname" : {
          "type" : "keyword"
        },
        "cnameCnt" : {
          "type" : "long"
        },
        "realm" : {
          "type" : "keyword"
        },
        "realmCnt" : {
          "type" : "long"
        },
        "sname" : {
          "type" : "keyword"
        },
        "snameCnt" : {
          "type" : "long"
        }
      }
    },
    "lastPacket" : {
      "type" : "date"
    },
    "ldap" : {
      "properties" : {
        "authtype" : {
          "type" : "keyword"
        },
        "authtypeCnt" : {
          "type" : "long"
        },
        "bindname" : {
          "type" : "keyword"
        },
        "bindnameCnt" : {
          "type" : "long"
        }
      }
    },
    "length" : {
      "type" : "long"
    },
    "mysql" : {
      "properties" : {
        "user" : {
          "type" : "keyword"
        },
        "version" : {
          "type" : "keyword"
        }
      }
    },
    "node" : {
      "type" : "keyword"
    },
    "oracle" : {
      "properties" : {
        "host" : {
          "type" : "keyword",
          "copy_to" : [
            "oracle.hostTokens"
          ]
        },
        "hostTokens" : {
          "type" : "text",
          "norms" : false,
          "analyzer" : "wordSplit"
        },
        "service" : {
          "type" : "keyword"
        },
        "user" : {
          "type" : "keyword"
        }
      }
    },
    "packetLen" : {
      "type" : "integer",
      "index" : false
    },
    "packetPos" : {
      "type" : "long",
      "index" : false
    },
    "postgresql" : {
      "properties" : {
        "app" : {
          "type" : "keyword"
        },
        "db" : {
          "type" : "keyword"
        },
        "user" : {
          "type" : "keyword"
        }
      }
    },
    "protocol" : {
      "type" : "keyword"
    },
    "protocolCnt" : {
      "type" : "long"
    },
    "quic" : {
      "properties" : {
        "host" : {
          "type" : "keyword",
          "copy_to" : [
            "quic.hostTokens"
          ]
        },
        "hostCnt" : {
          "type" : "long"
        },
        "hostTokens" : {
          "type" : "text",
          "norms" : false,
          "analyzer" : "wordSplit"
        },
        "useragent" : {
          "type" : "keyword",
          "copy_to" : [
            "quic.useragentTokens"
          ]
        },
        "useragentCnt" : {
          "type" : "long"
        },
        "useragentTokens" : {
          "type" : "text",
          "norms" : false,
          "analyzer" : "wordSplit"
        },
        "version" : {
          "type" : "keyword"
        },
        "versionCnt" : {
          "type" : "long"
        }
      }
    },
    "radius" : {
      "properties" : {
        "framedASN" : {
          "type" : "keyword"
        },
        "framedGEO" : {
          "type" : "keyword"
        },
        "framedIp" : {
          "type" : "ip"
        },
        "framedIpCnt" : {
          "type" : "long"
        },
        "framedRIR" : {
          "type" : "keyword"
        },
        "mac" : {
          "type" : "keyword"
        },
        "macCnt" : {
          "type" : "long"
        },
        "user" : {
          "type" : "keyword"
        }
      }
    },
    "rootId" : {
      "type" : "keyword"
    },
    "segmentCnt" : {
      "type" : "long"
    },
    "smb" : {
      "properties" : {
        "filename" : {
          "type" : "keyword"
        },
        "filenameCnt" : {
          "type" : "long"
        },
        "host" : {
          "type" : "keyword",
          "copy_to" : [
            "smb.hostTokens"
          ]
        }
      }
    },
    "socks" : {
      "properties" : {
        "ASN" : {
          "type" : "keyword"
        },
        "GEO" : {
          "type" : "keyword"
        },
        "RIR" : {
          "type" : "keyword"
        },
        "host" : {
          "type" : "keyword",
          "copy_to" : [
            "socks.hostTokens"
          ]
        },
        "ip" : {
          "type" : "ip"
        },
        "port" : {
          "type" : "long"
        },
        "user" : {
          "type" : "keyword"
        }
      }
    },
    "srcOui" : {
      "type" : "keyword"
    },
    "srcOuiCnt" : {
      "type" : "long"
    },
    "srcPayload8" : {
      "type" : "keyword"
    },
    "srcRIR" : {
      "type" : "keyword"
    },
    "ssh" : {
      "properties" : {
        "hassh" : {
          "type" : "keyword"
        },
        "hasshCnt" : {
          "type" : "long"
        },
        "hasshServer" : {
          "type" : "keyword"
        },
        "hasshServerCnt" : {
          "type" : "long"
        },
        "key" : {
          "type" : "keyword"
        },
        "keyCnt" : {
          "type" : "long"
        },
        "version" : {
          "type" : "keyword"
        },
        "versionCnt" : {
          "type" : "long"
        }
      }
    },
    "suricata" : {
      "properties" : {
        "action" : {
          "type" : "keyword"
        },
        "actionCnt" : {
          "type" : "long"
        },
        "category" : {
          "type" : "keyword"
        },
        "categoryCnt" : {
          "type" : "long"
        },
        "flowId" : {
          "type" : "keyword"
        },
        "flowIdCnt" : {
          "type" : "long"
        },
        "gid" : {
          "type" : "long"
        },
        "gidCnt" : {
          "type" : "long"
        },
        "severity" : {
          "type" : "long"
        },
        "severityCnt" : {
          "type" : "long"
        },
        "signature" : {
          "type" : "keyword"
        },
        "signatureCnt" : {
          "type" : "long"
        },
        "signatureId" : {
          "type" : "long"
        },
        "signatureIdCnt" : {
          "type" : "long"
        }
      }
    },
    "tags" : {
      "type" : "keyword"
    },
    "tagsCnt" : {
      "type" : "long"
    },
    "tcpflags" : {
      "properties" : {
        "ack" : {
          "type" : "long"
        },
        "dstZero" : {
          "type" : "long"
        },
        "fin" : {
          "type" : "long"
        },
        "psh" : {
          "type" : "long"
        },
        "rst" : {
          "type" : "long"
        },
        "srcZero" : {
          "type" : "long"
        },
        "syn" : {
          "type" : "long"
        },
        "syn-ack" : {
          "type" : "long"
        },
        "urg" : {
          "type" : "long"
        }
      }
    },
    "tls" : {
      "properties" : {
        "cipher" : {
          "type" : "keyword"
        },
        "cipherCnt" : {
          "type" : "long"
        },
        "dstSessionId" : {
          "type" : "keyword"
        },
        "ja3" : {
          "type" : "keyword"
        },
        "ja3Cnt" : {
          "type" : "long"
        },
        "ja3s" : {
          "type" : "keyword"
        },
        "ja3sCnt" : {
          "type" : "long"
        },
        "srcSessionId" : {
          "type" : "keyword"
        },
        "version" : {
          "type" : "keyword"
        },
        "versionCnt" : {
          "type" : "long"
        }
      }
    },
    "totDataBytes" : {
      "type" : "long"
    },
    "user" : {
      "type" : "keyword"
    },
    "userCnt" : {
      "type" : "long"
    }
  }
}
';

$REPLICAS = 0 if ($REPLICAS < 0);
my $shardsPerNode = ceil($SHARDS * ($REPLICAS+1) / $main::numberOfNodes);
$shardsPerNode = $SHARDSPERNODE if ($SHARDSPERNODE eq "null" || $SHARDSPERNODE > $shardsPerNode);

my $settings = '';
if ($DOHOTWARM) {
  $settings .= ',
      "routing.allocation.require.molochtype": "hot"';
}

if ($DOILM) {
  $settings .= qq/,
      "lifecycle.name": "${PREFIX}molochsessions"/;
}

    my $template = '
{
  "index_patterns": "' . $PREFIX . 'sessions3-*",
  "settings": {
    "index": {
      "routing.allocation.total_shards_per_node": ' . $shardsPerNode . $settings . ',
      "refresh_interval": "' . $REFRESH . 's",
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
  "mappings":' . $mapping . ',
  "order": 99
}';

    logmsg "Creating sessions template\n" if ($verbose > 0);
    esPut("/_template/${PREFIX}sessions3_template?master_timeout=${ESTIMEOUT}s&pretty", $template);

    my $indices = esGet("/${PREFIX}sessions3-*/_alias", 1);

    if ($UPGRADEALLSESSIONS) {
        logmsg "Updating sessions3 mapping for ", scalar(keys %{$indices}), " indices\n" if (scalar(keys %{$indices}) != 0);
        foreach my $i (keys %{$indices}) {
            progress("$i ");
            esPut("/$i/_mapping?master_timeout=${ESTIMEOUT}s", $mapping, 1);
        }
        logmsg "\n" if (scalar(keys %{$indices}) != 0);
    }

    sessions3ECSTemplate();
}

################################################################################
sub historyUpdate
{
    my $mapping = '
{
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
      "type": "date",
      "format": "epoch_second"
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
      "dynamic": "true",
      "enabled": "false"
    },
    "forcedExpression": {
      "type": "keyword"
    },
    "esQuery": {
      "type": "text",
      "index": "false"
    },
    "esQueryIndices": {
      "type": "text",
      "index": "false"
    }
  }
}';

my $settings = '';
if ($DOILM) {
  $settings .= qq/"lifecycle.name": "${PREFIX}molochhistory",
/;
}

 my $template = qq/
{
  "index_patterns": "${PREFIX}history_v1-*",
  "settings": {
      ${settings}
      "number_of_shards": 1,
      "number_of_replicas": 0,
      "auto_expand_replicas": "0-1"
    },
  "mappings": ${mapping}
}/;

logmsg "Creating history template\n" if ($verbose > 0);
esPut("/_template/${PREFIX}history_v1_template?master_timeout=${ESTIMEOUT}s&pretty", $template);

my $indices = esGet("/${PREFIX}history_v1-*/_alias", 1);

if ($UPGRADEALLSESSIONS) {
    logmsg "Updating history mapping for ", scalar(keys %{$indices}), " indices\n" if (scalar(keys %{$indices}) != 0);
    foreach my $i (keys %{$indices}) {
        progress("$i ");
        esPut("/$i/_mapping?master_timeout=${ESTIMEOUT}s", $mapping, 1);
    }
    logmsg "\n" if (scalar(keys %{$indices}) != 0);
}

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

  logmsg "Creating hunts_v30 index\n" if ($verbose > 0);
  esPut("/${PREFIX}hunts_v30?master_timeout=${ESTIMEOUT}s", $settings);
  esAlias("add", "hunts_v30", "hunts");
  huntsUpdate();
}

sub huntsUpdate
{
    my $mapping = '
{
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
        },
        "time": {
          "type": "date"
        },
        "node": {
          "type": "keyword"
        }
      }
    },
    "notifier": {
      "type": "keyword"
    },
    "unrunnable": {
      "type": "boolean"
    },
    "failedSessionIds": {
      "type": "keyword"
    },
    "users": {
      "type": "keyword"
    },
    "removed": {
      "type": "boolean"
    },
    "description": {
      "type": "text",
      "index": "false"
    },
    "roles": {
      "type": "keyword"
    }
  }
}';

logmsg "Setting hunts_v30 mapping\n" if ($verbose > 0);
esPut("/${PREFIX}hunts_v30/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping);
}
################################################################################

################################################################################
sub lookupsCreate
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

  logmsg "Creating lookups_v30 index\n" if ($verbose > 0);
  esPut("/${PREFIX}lookups_v30?master_timeout=${ESTIMEOUT}s", $settings);
  esAlias("add", "lookups_v30", "lookups");
  lookupsUpdate();
}

sub lookupsUpdate
{
    my $mapping = '
{
  "_source": {"enabled": "true"},
  "dynamic": "strict",
  "properties": {
    "userId": {
      "type": "keyword"
    },
    "name": {
      "type": "keyword"
    },
    "description": {
      "type": "keyword"
    },
    "number": {
      "type": "integer"
    },
    "ip": {
      "type": "keyword"
    },
    "string": {
      "type": "keyword"
    },
    "locked": {
      "type": "boolean"
    },
    "users": {
      "type": "keyword"
    },
    "roles": {
      "type": "keyword"
    }
  }
}';

logmsg "Setting lookups_v30 mapping\n" if ($verbose > 0);
esPut("/${PREFIX}lookups_v30/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping);

# update shared=true to roles=['arkimeUser']
my $json = esGet("/${PREFIX}lookups/_search?q=shared:true&size=1000");
foreach my $j (@{$json->{hits}->{hits}}) {
   delete $j->{_source}->{shared};
   $j->{_source}->{roles} = ["arkimeUser"];
   esPut("/${PREFIX}lookups/_doc/$j->{_id}", to_json($j->{_source}));
}
}
################################################################################

################################################################################
sub notifiersCreate
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

  logmsg "Creating notifiers_v40 index\n" if ($verbose > 0);
  esPut("/${PREFIX}notifiers_v40?master_timeout=${ESTIMEOUT}s", $settings);
  esAlias("add", "notifiers_v40", "notifiers");
  notifiersUpdate();
}

sub notifiersUpdate
{
    my $mapping = '
{
  "_source": {"enabled": "true"},
  "dynamic": "true",
  "dynamic_templates": [
    {
      "string_template": {
        "match_mapping_type": "string",
        "mapping": {
          "type": "keyword"
        }
      }
    }
  ],
  "properties": {
    "name": {
      "type": "keyword"
    },
    "users": {
      "type": "keyword"
    },
    "roles": {
      "type": "keyword"
    },
    "user": {
      "type": "keyword"
    },
    "type": {
      "type": "keyword"
    },
    "created": {
      "type": "date"
    },
    "updated": {
      "type": "date"
    }
  }
}';

logmsg "Setting notifiers_v40 mapping\n" if ($verbose > 0);
esPut("/${PREFIX}notifiers_v40/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping);
}

sub notifiersMove
{
# add the notifiers from the _moloch_shared user to the new notifiers index
    my $sharedUser = esGet("/${PREFIX}users/_source/_moloch_shared", 1);
    my @notifiers = keys %{$sharedUser->{notifiers}};

    foreach my $n (@notifiers) {
        my $notifier = $sharedUser->{notifiers}{$n};
        $notifier->{users} = "";
        $notifier->{roles} = ["arkimeUser", "parliamentUser"];
        my $name = $notifier->{name};
        esPost("/${PREFIX}notifiers/_doc/${name}", to_json($notifier));
    }

# remove notifiers from the _moloch_shared user
    delete $sharedUser->{notifiers};
    esPut("/${PREFIX}users/_doc/_moloch_shared", to_json($sharedUser));
}
################################################################################

################################################################################
sub viewsCreate
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

  logmsg "Creating views_v40 index\n" if ($verbose > 0);
  esPut("/${PREFIX}views_v40?master_timeout=${ESTIMEOUT}s", $settings);
  esAlias("add", "views_v40", "views");
  viewsUpdate();
}

sub viewsUpdate
{
    my $mapping = '
{
  "_source": {"enabled": "true"},
  "dynamic": "strict",
  "properties": {
    "name": {
      "type": "keyword"
    },
    "users": {
      "type": "keyword"
    },
    "roles": {
      "type": "keyword"
    },
    "user": {
      "type": "keyword"
    },
    "expression": {
      "type": "keyword"
    },
    "sessionsColConfig": {
      "type": "object",
      "dynamic": "true",
      "enabled": "false"
    }
  }
}';

logmsg "Setting views_v40 mapping\n" if ($verbose > 0);
esPut("/${PREFIX}views_v40/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping);
}

sub viewsMove
{
# add the views from all users to the new views index
  my $users = esGet("/${PREFIX}users/_search?size=1000");

  foreach my $user (@{$users->{hits}->{hits}}) {
      my @views = keys %{$user->{_source}->{views}};

      foreach my $v (@views) {
          my $view = $user->{_source}->{views}{$v};
          $view->{users} = "";
          $view->{name} = $v;
          if ($view->{shared}) {
            $view->{roles} = ["arkimeUser"];
          }
          delete $view->{shared};
          if (!exists $view->{user}) {
            $view->{user} = $user->{_source}->{userId};
          }
          esPost("/${PREFIX}views/_doc", to_json($view));
      }

      # update the user to delete views
      delete $user->{_source}->{views};
      my $userId = $user->{_id};
      esPut("/${PREFIX}users/_doc/${userId}", to_json($user->{_source}));
  }

  # delete the _moloch_shared user
  esDelete("/${PREFIX}users/_doc/_moloch_shared", 1);
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

    logmsg "Creating users_v30 index\n" if ($verbose > 0);
    esPut("/${PREFIX}users_v30?master_timeout=${ESTIMEOUT}s", $settings);
    esAlias("add", "users_v30", "users");
    usersUpdate();
}
################################################################################
sub usersUpdate
{
    my $mapping = '
{
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
    },
    "cont3xt": {
      "type": "object",
      "dynamic": "true",
      "enabled": "false"
    },
    "roles": {
      "type": "keyword"
    },
    "roleAssigners": {
      "type": "keyword"
    }
  }
}';

    logmsg "Setting users_v30 mapping\n" if ($verbose > 0);
    esPut("/${PREFIX}users_v30/_mapping?master_timeout=${ESTIMEOUT}s&pretty", $mapping);
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

    if (esCheckAlias($alias, $newName) && esIndexExists($newName)) {
        logmsg ("SKIPPING - $alias already points to $newName\n");
        return;
    }

    if (!esIndexExists($oldName)) {
        die "ERROR - $oldName doesn't exist!";
    }

    $createFunction->();
    esAlias("remove", $oldName, $alias, 1);
    esCopy($oldName, $newName);
    esDelete("/${oldName}", 1);
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

  return 0 if ($index !~ /sessions[23]-(.*)$/);
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
        return sprintf("${prefix}%02d%02d%02dh%02d", $t[5] % 100, $t[4]+1, $t[3], $t[2]);
    }

    if ($type =~ /^hourly([23468])$/) {
        my $n = int($1);
        return sprintf("${prefix}%02d%02d%02dh%02d", $t[5] % 100, $t[4]+1, $t[3], int($t[2]/$n)*$n);
    }

    if ($type eq "hourly12") {
        return sprintf("${prefix}%02d%02d%02dh%02d", $t[5] % 100, $t[4]+1, $t[3], int($t[2]/12)*12);
    }

    if ($type eq "daily") {
        return sprintf("${prefix}%02d%02d%02d", $t[5] % 100, $t[4]+1, $t[3]);
    }

    if ($type eq "weekly") {
        return sprintf("${prefix}%02dw%02d", $t[5] % 100, int($t[7]/7));
    }

    if ($type eq "monthly") {
        return sprintf("${prefix}%02dm%02d", $t[5] % 100, $t[4]+1);
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

    $version = esGet("/_template/${OLDPREFIX}sessions2_template,${PREFIX}sessions3_template?filter_path=**._meta", 1);

    if (defined $version &&
        exists $version->{"${PREFIX}sessions3_template"} &&
        exists $version->{"${PREFIX}sessions3_template"}->{mappings}->{_meta} &&
        exists $version->{"${PREFIX}sessions3_template"}->{mappings}->{_meta}->{molochDbVersion}
    ) {
        $main::versionNumber = $version->{"${PREFIX}sessions3_template"}->{mappings}->{_meta}->{molochDbVersion};
        return;
    }

    if (defined $version &&
        exists $version->{"${OLDPREFIX}sessions2_template"} &&
        exists $version->{"${OLDPREFIX}sessions2_template"}->{mappings}->{_meta} &&
        exists $version->{"${OLDPREFIX}sessions2_template"}->{mappings}->{_meta}->{molochDbVersion}
    ) {
        $main::versionNumber = $version->{"${OLDPREFIX}sessions2_template"}->{mappings}->{_meta}->{molochDbVersion};
        return;
    }

    logmsg "This is a fresh Arkime install\n" if ($loud);
    $main::versionNumber = -1;
    if ($loud && $ARGV[1] !~ "init") {
        die "Looks like Arkime wasn't installed, must do init"
    }
}
################################################################################
sub dbCheckForActivity {
my ($prefix) = @_;

    logmsg "This upgrade requires all capture nodes to be stopped.  Checking\n";
    my $json1 = esGet("/${prefix}stats/stat/_search?size=1000&rest_total_hits_as_int=true");
    sleep(6);
    my $json2 = esGet("/${prefix}stats/stat/_search?size=1000&rest_total_hits_as_int=true");
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
    my @parts = split(/[-.]/, $esversion->{version}->{number});
    $main::esVersion = int($parts[0]*100*100) + int($parts[1]*100) + int($parts[2]);

    if ($esversion->{version}->{distribution} eq "opensearch") {
        if ($main::esVersion < 1000) {
            logmsg("Currently using Opensearch version ", $esversion->{version}->{number}, " which isn't supported\n",
                  "* < 1.0.0 is not supported\n"
                  );
            exit (1)
        }
    } else {
        if ($main::esVersion < 71000) {
            logmsg("Currently using Elasticsearch version ", $esversion->{version}->{number}, " which isn't supported\n",
                  "* < 7.10.0 is not supported\n",
                  "\n",
                  "Instructions: https://molo.ch/faq#how-do-i-upgrade-elasticsearch\n",
                  "Make sure to restart any viewer or capture after upgrading!\n"
                 );
            exit (1)
        }

        if ($main::esVersion < 71002) {
            logmsg("Currently using Elasticsearch version ", $esversion->{version}->{number}, " 7.10.2 or newer is recommended\n");
        }
    }

    my $error = 0;
    my $nodes = esGet("/_nodes?flat_settings");
    my $nodeStats = esGet("/_nodes/stats");

    foreach my $key (sort {$nodes->{nodes}->{$a}->{name} cmp $nodes->{nodes}->{$b}->{name}} keys %{$nodes->{nodes}}) {
        next if (!('data' ~~ @{$nodes->{nodes}->{$key}->{roles}}));
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
sub checkForOld7Indices {
    my $result = esGet("/_all/_settings/index.version.created?pretty");
    my $found = 0;

    while ( my ($key, $value) = each (%{$result})) {
        if ($value->{settings}->{index}->{version}->{created} < 7000000) {
            logmsg "WARNING: You must delete index '$key' before upgrading to ES 8\n";
            $found = 1;
        }
    }

    if ($found) {
        logmsg "\nYou MUST delete (and optionally re-add) the indices above while still on ES 7.x otherwise ES 8.x will NOT start.\n\n";
    }
}
################################################################################
sub progress {
    my ($msg) = @_;
    if ($verbose == 1) {
        local $| = 1;
        logmsg ".";
    } elsif ($verbose >= 2) {
        local $| = 1;
        logmsg "$msg";
    }
}
################################################################################
sub optimizeOther {
    logmsg "Optimizing Admin Indices\n";
    esForceMerge("${PREFIX}stats_v30,${PREFIX}dstats_v30,${PREFIX}fields_v30,${PREFIX}files_v30,${PREFIX}sequence_v30,${PREFIX}users_v30,${PREFIX}queries_v30,${PREFIX}hunts_v30,${PREFIX}lookups_v30,${PREFIX}notifiers_v40,${PREFIX}views_v40", 1, 0);
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
        } elsif ($ARGV[$pos] eq "--refresh") {
            $pos++;
            $REFRESH = int($ARGV[$pos]);
        } elsif ($ARGV[$pos] eq "--history") {
            $pos++;
            $HISTORY = int($ARGV[$pos]);
        } elsif ($ARGV[$pos] eq "--segments") {
            $pos++;
            $SEGMENTS = int($ARGV[$pos]);
        } elsif ($ARGV[$pos] eq "--segmentsmin") {
            $pos++;
            $SEGMENTSMIN = int($ARGV[$pos]);
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
        } elsif ($ARGV[$pos] eq "--ilm") {
            $DOILM = 1;
        } elsif ($ARGV[$pos] eq "--warmafter") {
            $pos++;
            $WARMAFTER = int($ARGV[$pos]);
            $WARMKIND = $ARGV[2];
            if (substr($ARGV[$pos], -6) eq "hourly") {
                $WARMKIND = "hourly";
            } elsif (substr($ARGV[$pos], -5) eq "daily") {
                $WARMKIND = "daily";
            }
        } elsif ($ARGV[$pos] eq "--optimizewarm") {
            $OPTIMIZEWARM = 1;
        } elsif ($ARGV[$pos] eq "--shareUsers") {
            $pos++;
            $SHAREUSERS = $ARGV[$pos];
        } elsif ($ARGV[$pos] eq "--shareRoles") {
            $pos++;
            $SHAREROLES = $ARGV[$pos];
        } elsif ($ARGV[$pos] eq "--locked") {
            $LOCKED = 1;
        } elsif ($ARGV[$pos] eq "--gz") {
            $GZ = 1;
        } elsif ($ARGV[$pos] eq "--type") {
            $pos++;
            $TYPE = $ARGV[$pos];
        } elsif ($ARGV[$pos] eq "--description") {
            $pos++;
            $DESCRIPTION = $ARGV[$pos];
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
        $PREFIX .= "_" if ($PREFIX ne "" && $PREFIX !~ /_$/);
        $OLDPREFIX = $PREFIX;
    } elsif ($ARGV[0] =~ /-n$/) {
        $NOCHANGES = 1;
    } elsif ($ARGV[0] =~ /--insecure$/) {
        $SECURE = 0;
    } elsif ($ARGV[0] =~ /--clientcert$/) {
        $CLIENTCERT = $ARGV[1];
        shift @ARGV;
    } elsif ($ARGV[0] =~ /--clientkey$/) {
        $CLIENTKEY = $ARGV[1];
        shift @ARGV;
    } elsif ($ARGV[0] =~ /--timeout$/) {
        $ESTIMEOUT = int($ARGV[1]);
        shift @ARGV;
    } elsif ($ARGV[0] =~ /(--esapikey|--elasticsearchAPIKey)$/) {
        $ESAPIKEY = $ARGV[1];
        shift @ARGV;
    } elsif ($ARGV[0] =~ /--esuser$/) {
        $USERPASS = $ARGV[1];
        shift @ARGV;
        if ($USERPASS !~ ':') {
            system ("stty -echo 2> /dev/null");
            $USERPASS .= ':' . waitForRE(qr/^.{6,}$/, "Enter 6+ character password for $USERPASS:");
            system ("stty echo 2> /dev/null");
        }
        $USERPASS = encode_base64($USERPASS);
    } else {
        showHelp("Unknkown global option $ARGV[0]")
    }
    shift @ARGV;
}

$PREFIX = "arkime_" if (! defined $PREFIX);

showHelp("Help:") if ($ARGV[1] =~ /^help$/);
showHelp("Missing arguments") if (@ARGV < 2);
showHelp("Unknown command '$ARGV[1]'") if ($ARGV[1] !~ /^(init|initnoprompt|clean|info|wipe|upgrade|upgradenoprompt|disable-?users|set-?shortcut|users-?import|import|restore|users-?export|export|repair|backup|expire|rotate|optimize|optimize-admin|mv|rm|rm-?missing|rm-?node|add-?missing|field|force-?put-?version|sync-?files|hide-?node|unhide-?node|add-?alias|set-?replicas|set-?shards-?per-?node|set-?allocation-?enable|allocate-?empty|unflood-?stage|shrink|ilm|recreate-users|recreate-stats|recreate-dstats|recreate-fields|update-fields|update-history|reindex|force-sessions3-update|es-adduser|es-passwd|es-addapikey)$/);
showHelp("Missing arguments") if (@ARGV < 3 && $ARGV[1] =~ /^(users-?import|import|users-?export|backup|restore|rm|rm-?missing|rm-?node|hide-?node|unhide-?node|set-?allocation-?enable|unflood-?stage|reindex|es-adduser|es-addapikey)$/);
showHelp("Missing arguments") if (@ARGV < 4 && $ARGV[1] =~ /^(field|export|add-?missing|sync-?files|add-?alias|set-?replicas|set-?shards-?per-?node|set-?shortcut|ilm)$/);
showHelp("Missing arguments") if (@ARGV < 5 && $ARGV[1] =~ /^(allocate-?empty|set-?shortcut|shrink)$/);
showHelp("Must have both <old fn> and <new fn>") if (@ARGV < 4 && $ARGV[1] =~ /^(mv)$/);
showHelp("Must have both <type> and <num> arguments") if (@ARGV < 4 && $ARGV[1] =~ /^(rotate|expire)$/);

parseArgs(2) if ($ARGV[1] =~ /^(init|initnoprompt|upgrade|upgradenoprompt|clean|wipe|optimize)$/);
parseArgs(3) if ($ARGV[1] =~ /^(restore|backup)$/);

$ESTIMEOUT = 240 if ($ESTIMEOUT < 240 && $ARGV[1] =~ /^(init|initnoprompt|upgrade|upgradenoprompt|clean|shrink|ilm)$/);

$main::userAgent = LWP::UserAgent->new(timeout => $ESTIMEOUT + 5, keep_alive => 5);

if ($ESAPIKEY ne "") {
    $main::userAgent->default_header('Authorization' => "ApiKey $ESAPIKEY");
} elsif ($USERPASS ne "") {
    $main::userAgent->default_header('Authorization' => "Basic $USERPASS");
}

if ($CLIENTCERT ne "") {
    $main::userAgent->ssl_opts(
        SSL_verify_mode => $SECURE,
        verify_hostname=> $SECURE,
        SSL_cert_file => $CLIENTCERT,
        SSL_key_file => $CLIENTKEY
    )
} else {
    $main::userAgent->ssl_opts(
        SSL_verify_mode => $SECURE,
        verify_hostname=> $SECURE
    )
}

if ($ARGV[0] =~ /^urlinfile:\/\//) {
    open( my $file, substr($ARGV[0], 12)) or die "Couldn't open file ", substr($ARGV[0], 12);
    $main::elasticsearch = <$file>;
    chomp $main::elasticsearch;
    close ($file);
} elsif ($ARGV[0] =~ /^http/) {
    $main::elasticsearch = $ARGV[0];
} else {
    $main::elasticsearch = "http://$ARGV[0]";
}

if ($ARGV[1] =~ /^(users-?import|import)$/) {
    my $fh;
    if ($ARGV[2] =~ /\.gz$/) {
        $fh = new IO::Uncompress::Gunzip $ARGV[2] or die "gunzip failed: $GunzipError\n";
    } else {
        open($fh, "<", $ARGV[2]) or die "cannot open < $ARGV[2]: $!";
    }
    my $data = do { local $/; <$fh> };
    # Use version/version_type instead of _version/_version_type
    $data =~ s/, "_version": (\d+), "_version_type"/, "version": \1, "version_type"/g;
    # Remove type from older backups
    $data =~ s/, "_type": .*, "_id":/, "_id":/g;
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
        print $fh "{\"index\": {\"_index\": \"${PREFIX}${index}\", \"_id\": \"$hit->{_id}\", \"version\": $hit->{_version}, \"version_type\": \"external\"}}\n";
        if (exists $hit->{_source}) {
            print $fh to_json($hit->{_source}) . "\n";
        } else {
            print $fh "{}\n";
        }
    }
    close($fh);
    exit 0;
} elsif ($ARGV[1] =~ /^backup$/) {
    sub bopen {
        my ($index) = @_;
        if ($GZ) {
            return new IO::Compress::Gzip "$ARGV[2].${PREFIX}${index}.json.gz" or die "cannot open $ARGV[2].${PREFIX}${index}.json.gz: $GzipError\n";
        } else {
            open(my $fh, ">", "$ARGV[2].${PREFIX}${index}.json") or die "cannot open > $ARGV[2].${PREFIX}${index}.json: $!";
            return $fh;
        }
    }

    my @indexes = ("users", "sequence", "stats", "queries", "files", "fields", "dstats", "hunts", "lookups", "notifiers", "views");
    logmsg "Exporting documents...\n";
    foreach my $index (@indexes) {
        my $data = esScroll($index, "", '{"version": true}');
        next if (scalar(@{$data}) == 0);
        my $fh = bopen($index);
        foreach my $hit (@{$data}) {
            print $fh "{\"index\": {\"_index\": \"${PREFIX}${index}\", \"_id\": \"$hit->{_id}\", \"version\": $hit->{_version}, \"version_type\": \"external\"}}\n";
            if (exists $hit->{_source}) {
                print $fh to_json($hit->{_source}) . "\n";
            } else {
                print $fh "{}\n";
            }
        }
        close($fh);
    }
    logmsg "Exporting templates...\n";
    my @templates = ("sessions3_template", "history_v1_template");
    foreach my $template (@templates) {
        my $data = esGet("/_template/${PREFIX}${template}");
        my @name = split(/_/, $template);
        my $fh = bopen("template");
        print $fh to_json($data);
        close($fh);
    }
    logmsg "Exporting settings...\n";
    foreach my $index (@indexes) {
        my $data = esGet("/${PREFIX}${index}/_settings");
        my $fh = bopen("${index}.settings");
        print $fh to_json($data);
        close($fh);
    }
    logmsg "Exporting mappings...\n";
    foreach my $index (@indexes) {
        my $data = esGet("/${PREFIX}${index}/_mappings");
        my $fh = bopen("${index}.mappings");
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
    my $fh = bopen("aliases");
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

    $SEGMENTSMIN = $SEGMENTS if ($SEGMENTSMIN == -1);

    # First handle sessions expire
    my $indicesa = esGet("/_cat/indices/${OLDPREFIX}sessions2-*,${PREFIX}sessions3-*?format=json", 1);
    my %indices = map { $_->{index} => $_ } @{$indicesa};

    my $endTime = time();
    my $endTimeIndex2 = time2index($ARGV[2], "${OLDPREFIX}sessions2-", $endTime, "");
    delete $indices{$endTimeIndex2}; # Don't optimize current index

    my $endTimeIndex3 = time2index($ARGV[2], "${PREFIX}sessions3-", $endTime);
    delete $indices{$endTimeIndex3}; # Don't optimize current index

    my @startTime = kind2time($ARGV[2], int($ARGV[3]));

    parseArgs(4);

    my $startTime = mktimegm(@startTime);
    my @warmTime = kind2time($WARMKIND, $WARMAFTER);
    my $warmTime = mktimegm(@warmTime);
    my $optimizecnt = 0;
    my $warmcnt = 0;
    my @indiceskeys = sort (keys %indices);

    foreach my $i (@indiceskeys) {
        my $t = index2time($i);
        if ($t >= $startTime) {
            $indices{$i}->{OPTIMIZEIT} = 1;
            $optimizecnt++;
        }
        if ($WARMAFTER != -1 && $t < $warmTime) {
            $indices{$i}->{WARMIT} = 1;
            $warmcnt++;
        }
    }

    my $nodes = esGet("/_nodes");
    $main::numberOfNodes = dataNodes($nodes->{nodes});
    my $shardsPerNode = ceil($SHARDS * ($REPLICAS+1) / $main::numberOfNodes);
    $shardsPerNode = $SHARDSPERNODE if ($SHARDSPERNODE eq "null" || $SHARDSPERNODE > $shardsPerNode);

    dbESVersion();
    optimizeOther() unless $NOOPTIMIZE ;
    logmsg sprintf ("Expiring %s sessions indices, %s optimizing %s, warming %s\n", commify(scalar(keys %indices) - $optimizecnt), $NOOPTIMIZE?"Not":"", commify($optimizecnt), commify($warmcnt));
    esPost("/_flush/synced", "", 1);

    @indiceskeys = reverse(@indiceskeys) if ($REVERSE);

    # Get all the settings at once, we use below to see if we need to change them
    my $settings = esGet("/_settings?flat_settings&master_timeout=${ESTIMEOUT}s", 1);

    # Find all the shards that have too many segments and increment the OPTIMIZEIT count or not warm
    my $shards = esGet("/_cat/shards/${OLDPREFIX}sessions2*,${PREFIX}sessions3*?h=i,sc&format=json");
    for my $i (@{$shards}) {
        # Not expiring and too many segments
        if (exists $indices{$i->{i}}->{OPTIMIZEIT} && defined $i->{sc} && int($i->{sc}) > $SEGMENTSMIN) {
            # Either not only optimizing warm or make sure we are green warm
            if (!$OPTIMIZEWARM ||
                ($settings->{$i->{i}}->{settings}->{"index.routing.allocation.require.molochtype"} eq "warm" && $indices{$i->{i}}->{health} eq "green")) {

                $indices{$i->{i}}->{OPTIMIZEIT}++;
            }
        }

        if (exists $indices{$i->{i}}->{WARMIT} && $settings->{$i->{i}}->{settings}->{"index.routing.allocation.require.molochtype"} ne "warm") {
            $indices{$i->{i}}->{WARMIT}++;
        }
    }

    foreach my $i (@indiceskeys) {
        progress("$i ");
        if (exists $indices{$i}->{OPTIMIZEIT}) {

            # 1 is set if it shouldn't be expired, > 1 means it needs to be optimized
            if ($indices{$i}->{OPTIMIZEIT} > 1) {
                esForceMerge($i, $SEGMENTS, 1) unless $NOOPTIMIZE;
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

        if ($indices{$i}->{WARMIT} > 1) {
            esPut("/$i/_settings?master_timeout=${ESTIMEOUT}s", '{"index": {"routing.allocation.require.molochtype": "warm"}}', 1);
        }
    }
    esPost("/_flush/synced", "", 1);

    # Now figure out history expire
    my $hindices = esGet("/${PREFIX}history_v1-*,${OLDPREFIX}history_v1-*/_alias", 1);

    my $endTimeIndex = time2index("weekly", "${OLDPREFIX}history_v1-", $endTime);
    delete $hindices->{$endTimeIndex};

    $endTimeIndex = time2index("weekly", "${PREFIX}history_v1-", $endTime);
    delete $hindices->{$endTimeIndex};

    @startTime = gmtime;
    $startTime[3] -= 7 * $HISTORY;

    $optimizecnt = 0;
    $startTime = mktimegm(@startTime);
    while ($startTime <= $endTime + 2*24*60*60) {
        my $iname = time2index("weekly", "${PREFIX}history_v1-", $startTime);
        my $inameold = time2index("weekly", "${OLDPREFIX}history_v1-", $startTime);
        if (exists $hindices->{$iname} && $hindices->{$iname}->{OPTIMIZEIT} != 1) {
            $hindices->{$iname}->{OPTIMIZEIT} = 1;
            $optimizecnt++;
        } elsif (exists $hindices->{"$iname-shrink"} && $hindices->{"$iname-shrink"}->{OPTIMIZEIT} != 1) {
            $hindices->{"$iname-shrink"}->{OPTIMIZEIT} = 1;
            $optimizecnt++;
        } elsif (exists $hindices->{$inameold} && $hindices->{$inameold}->{OPTIMIZEIT} != 1) {
            $hindices->{$inameold}->{OPTIMIZEIT} = 1;
            $optimizecnt++;
        } elsif (exists $hindices->{"$inameold-shrink"} && $hindices->{"$inameold-shrink"}->{OPTIMIZEIT} != 1) {
            $hindices->{"$inameold-shrink"}->{OPTIMIZEIT} = 1;
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
    esForceMerge("${OLDPREFIX}history_*,${PREFIX}history_*", 1, 1) unless $NOOPTIMIZE;
    esPost("/_flush/synced", "", 1);

    # Give the cluster a kick to rebalance
    esPost("/_cluster/reroute?master_timeout=${ESTIMEOUT}s&retry_failed");
    exit 0;
} elsif ($ARGV[1] eq "optimize") {
    my $indices = esGet("/${OLDPREFIX}sessions2-*,${PREFIX}sessions3*/_alias", 1);

    dbESVersion();
    $main::userAgent->timeout(7200);
    esPost("/_flush/synced", "", 1);
    optimizeOther();
    logmsg sprintf "Optimizing %s Session Indices\n", commify(scalar(keys %{$indices}));
    foreach my $i (sort (keys %{$indices})) {
        progress("$i ");
        esForceMerge($i, $SEGMENTS, 1);
    }
    esPost("/_flush/synced", "", 1);
    logmsg "Optimizing History\n";
    esForceMerge("${PREFIX}history_v1-*", 1, 1);
    logmsg "\n";
    exit 0;
} elsif ($ARGV[1] eq "optimize-admin") {
    $main::userAgent->timeout(7200);
    esPost("/_flush/synced", "", 1);
    optimizeOther();
    esForceMerge("${PREFIX}history_*", 1, 0);
    exit 0;
} elsif ($ARGV[1] =~ /^(disable-?users)$/) {
    showHelp("Invalid number of <days>") if (!defined $ARGV[2] || $ARGV[2] !~ /^[+-]?\d+$/);

    my $users = esGet("/${PREFIX}users/_search?size=1000&q=enabled:true+AND+createEnabled:false+AND+_exists_:lastUsed+AND+-userId:role\\:*");
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
            esPost("/${PREFIX}users/_update/$userId", '{"doc": {"enabled": false}}');
            $rmcount++;
        }
    }

    if ($rmcount == 0) {
      print "No users disabled\n";
    } else {
      print "$rmcount user(s) disabled\n";
    }

    exit 0;
} elsif ($ARGV[1] =~ /^(set-?shortcut)$/) {
    showHelp("Invalid name $ARGV[2], names cannot have special characters except '_'") if ($ARGV[2] =~ /[^-a-zA-Z0-9_]$/);
    showHelp("file '$ARGV[4]' not found") if (! -e $ARGV[4]);
    showHelp("file '$ARGV[4]' empty") if (-z $ARGV[4]);

    parseArgs(5);

    showHelp("Type must be ip, string, or number instead of $TYPE") if ($TYPE !~ /^(string|ip|number)$/);

    # read shortcuts file
    my $shortcutValues;
    open(my $fh, '<', $ARGV[4]);
    {
      local $/;
      $shortcutValues = <$fh>;
    }
    close($fh);

    my $shortcutsArray = [split /[\n,]/, $shortcutValues];

    my $shortcutName = $ARGV[2];
    my $shortcutUserId = $ARGV[3];

    my $shortcuts = esGet("/${PREFIX}lookups/_search?q=name:${shortcutName}");

    my $existingShortcut;
    foreach my $shortcut (@{$shortcuts->{hits}->{hits}}) {
      if ($shortcut->{_source}->{name} == $shortcutName) {
        $existingShortcut = $shortcut;
        last;
      }
    }

    # create shortcut object
    my $newShortcut;
    $newShortcut->{name} = $shortcutName;
    $newShortcut->{userId} = $shortcutUserId;
    $newShortcut->{$TYPE} = $shortcutsArray;
    if ($existingShortcut) { # use existing optional fields
      if ($existingShortcut->{_source}->{description}) {
        $newShortcut->{description} = $existingShortcut->{_source}->{description};
      }
      if ($existingShortcut->{_source}->{users}) {
        $newShortcut->{users} = $existingShortcut->{_source}->{users};
      }
      if ($existingShortcut->{_source}->{roles}) {
        $newShortcut->{roles} = $existingShortcut->{_source}->{roles};
      }
    }
    if ($DESCRIPTION) {
      $newShortcut->{description} = $DESCRIPTION;
    }
    if ($SHAREUSERS) {
      $newShortcut->{users} = [split /[,]/, $SHAREUSERS];
    }
    if ($SHAREROLES) {
      $newShortcut->{roles} = [split /[,]/, $SHAREROLES];
    }
    if ($LOCKED) {
      $newShortcut->{locked} = \1;
    }

    my $verb = "Created";
    if ($existingShortcut) { # update the shortcut
      $verb = "Updated";
      my $id = $existingShortcut->{_id};
      esPost("/${PREFIX}lookups/_doc/${id}", to_json($newShortcut));
    } else { # create the shortcut
      esPost("/${PREFIX}lookups/_doc", to_json($newShortcut));
    }

    # increment the _meta version by 1
    my $mapping = esGet("/${PREFIX}lookups/_mapping");
    my @indexes = keys %{$mapping};
    my $index = @indexes[0];
    my $meta = $mapping->{$index}->{mappings}->{_meta};


    $meta->{version}++;
    esPut("/${PREFIX}lookups/_mapping", to_json({"_meta" => $meta}));

    print "${verb} shortcut ${shortcutName}\n";

    exit 0;
} elsif ($ARGV[1] =~ /^(shrink)$/) {
    parseArgs(5);
    die "Only shrink history and sessions2 indices" if ($ARGV[2] !~ /(sessions2|sessions3|history)/);

    logmsg("Moving all shards for ${PREFIX}$ARGV[2] to $ARGV[3]\n");
    my $json = esPut("/${PREFIX}$ARGV[2]/_settings?master_timeout=${ESTIMEOUT}s", "{\"settings\": {\"index.routing.allocation.total_shards_per_node\": null, \"index.routing.allocation.require._name\" : \"$ARGV[3]\", \"index.blocks.write\": true}}");

    while (1) {
      $json = esGet("/_cluster/health?wait_for_no_relocating_shards=true&timeout=30s", 1);
      last if ($json->{relocating_shards} == 0);
      progress("Waiting for relocation to finish\n");
    }
    logmsg("Shrinking ${PREFIX}$ARGV[2] to ${PREFIX}$ARGV[2]-shrink\n");
    $json = esPut("/${PREFIX}$ARGV[2]/_shrink/${PREFIX}$ARGV[2]-shrink?master_timeout=${ESTIMEOUT}s&copy_settings=true", '{"settings": {"index.routing.allocation.require._name": null, "index.blocks.write": null, "index.codec": "best_compression", "index.number_of_shards": ' . $ARGV[4] . '}}');

    logmsg("Checking for completion\n");
    my $status = esGet("/${PREFIX}$ARGV[2]-shrink/_refresh", 0);
    my $status = esGet("/${PREFIX}$ARGV[2]-shrink/_flush", 0);
    my $status = esGet("/_stats/docs", 0);
    if ($status->{indices}->{"${PREFIX}$ARGV[2]-shrink"}->{primaries}->{docs}->{count} == $status->{indices}->{"${PREFIX}$ARGV[2]"}->{primaries}->{docs}->{count}) {
        logmsg("Deleting old index\n");
        esDelete("/${PREFIX}$ARGV[2]", 1);
        esPut("/${PREFIX}$ARGV[2]-shrink/_settings?master_timeout=${ESTIMEOUT}s", "{\"index.routing.allocation.total_shards_per_node\" : $SHARDSPERNODE}") if ($SHARDSPERNODE ne "null");
    } else {
        logmsg("Doc counts don't match, not deleting old index\n");
    }
    exit 0;
} elsif ($ARGV[1] eq "recreate-users") {
    waitFor("USERS", "This will delete and recreate the users index");
    esDelete("/${PREFIX}users_*", 1);
    esDelete("/${PREFIX}users", 1);
    usersCreate();
    exit 0;
} elsif ($ARGV[1] eq "recreate-stats") {
    waitFor("STATS", "This will delete and recreate the stats index, make sure no captures are running");
    esDelete("/${PREFIX}stats_*", 1);
    esDelete("/${PREFIX}stats", 1);
    statsCreate();
    exit 0;
} elsif ($ARGV[1] eq "recreate-dstats") {
    waitFor("DSTATS", "This will delete and recreate the dstats index, make sure no captures are running");
    esDelete("/${PREFIX}dstats_*", 1);
    esDelete("/${PREFIX}dstats", 1);
    dstatsCreate();
    exit 0;
} elsif ($ARGV[1] eq "recreate-fields") {
    waitFor("FIELDS", "This will delete and recreate the fields index, make sure no captures are running");
    esDelete("/${PREFIX}fields_*", 1);
    esDelete("/${PREFIX}fields", 1);
    fieldsCreate();
    exit 0;
} elsif ($ARGV[1] eq "update-fields") {
    fieldsUpdate();
    exit 0;
} elsif ($ARGV[1] eq "update-history") {
    historyUpdate();
    exit 0;
} elsif ($ARGV[1] =~ /^es-adduser$/) {
    my $password = waitForRE(qr/^.{6,}$/, "Enter 6+ character password for $ARGV[2]:");
    my $json = to_json({
      roles => ["superuser"],
      password => $password});
    esPost("/_security/user/$ARGV[2]", $json);
    exit 0;
} elsif ($ARGV[1] =~ /^es-passwd$/) {
    my $password = waitForRE(qr/^.{6,}$/, "Enter 6+ character password for $ARGV[2]:");
    my $json = to_json({password => $password});
    if (@ARGV < 3) {
        esPost("/_security/user/_password", $json);
    } else {
        esPost("/_security/user/$ARGV[2]/_password", $json);
    }
    exit 0;
} elsif ($ARGV[1] =~ /^es-addapikey$/) {
    my $json = to_json({ name => $ARGV[2] });
    $json = esPost("/_security/api_key", $json);
    print Dumper($json);
    print "\n";
    my $key = encode_base64($json->{id} . ':' . $json->{api_key});
    print "Add to config file:\nelasticsearchAPIKey=$key\n";
    exit 0;
} elsif ($ARGV[1] eq "force-sessions3-update") {
    my $nodes = esGet("/_nodes");
    $main::numberOfNodes = dataNodes($nodes->{nodes});
    if (int($SHARDS) > $main::numberOfNodes) {
        die "Can't set shards ($SHARDS) greater then the number of nodes ($main::numberOfNodes)";
    } elsif ($SHARDS == -1) {
        $SHARDS = $main::numberOfNodes;
        if ($SHARDS > 24) {
            logmsg "Setting # of shards to 24, use --shards for a different number\n";
            $SHARDS = 24;
        }
    }
    waitFor("SESSIONS3UPDATE", "This will update the sessions3 template and all sessions3 indices");
    sessions3Update();
    exit 0;
} elsif ($ARGV[1] eq "info") {
    dbVersion(0);
    my $esversion = dbESVersion();
    my $catNodes = esGet("/_cat/nodes?format=json&bytes=b&h=name,diskTotal,role");
    my $status = esGet("/_stats/docs,store", 1);
    my $minMax = esPost("/${OLDPREFIX}sessions2-*,${PREFIX}sessions3-*/_search?size=0", '{"aggs":{ "min" : { "min" : { "field" : "lastPacket" } }, "max" : { "max" : { "field" : "lastPacket" } } } }', 1);
    my $ilm = esGet("/_ilm/policy/${PREFIX}molochsessions", 1);

    my $sessions = 0;
    my $sessionsBytes = 0;
    my $sessionsTotalBytes = 0;

    my @sessions = grep /^${PREFIX}sessions[23]-/, keys %{$status->{indices}};
    foreach my $index (@sessions) {
        $sessions += $status->{indices}->{$index}->{primaries}->{docs}->{count};
        $sessionsBytes += int($status->{indices}->{$index}->{primaries}->{store}->{size_in_bytes});
        $sessionsTotalBytes += int($status->{indices}->{$index}->{total}->{store}->{size_in_bytes});
    }

    my $diskTotal = 0;
    my $dataNodes = 0;
    my $totalNodes = 0;
    foreach my $node (@{$catNodes}) {
        $totalNodes++;
        if ($node->{role} =~ /d/) {
            $diskTotal += $node->{diskTotal};
            $dataNodes++;
        }
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
        my $index = $status->{indices}->{$PREFIX.$name} || $status->{indices}->{$OLDPREFIX.$name};
        return if (!$index);
        printf "%-20s %17s (%s bytes)\n", $name . ":", commify($index->{primaries}->{docs}->{count}), commify($index->{primaries}->{store}->{size_in_bytes});
    }

    printf "Cluster Name:        %17s\n", $esversion->{cluster_name};
    printf "ES Version:          %17s\n", $esversion->{version}->{number};
    printf "DB Version:          %17s\n", $main::versionNumber;
    printf "ES Data Nodes:       %17s/%s\n", commify($dataNodes), commify($totalNodes);
    printf "Sessions Indices:    %17s\n", commify(scalar(@sessions));
    printf "Sessions:            %17s (%s bytes)\n", commify($sessions), commify($sessionsBytes);
    if (scalar(@sessions) > 0 && $dataNodes > 0) {
        printf "Sessions Density:    %17s (%s bytes)\n", commify(int($sessions/($dataNodes*scalar(@sessions)))),
                                                       commify(int($sessionsBytes/($dataNodes*scalar(@sessions))));
        my $days =  (int($minMax->{aggregations}->{max}->{value}) - int($minMax->{aggregations}->{min}->{value}))/(24*60*60*1000);

        printf "MB per Day:          %17s\n", commify(int($sessionsTotalBytes/($days*1000*1000))) if ($days > 0);
        printf "Sessions Days:       %17.2f (%s - %s)\n", $days, $minMax->{aggregations}->{min}->{value_as_string}, $minMax->{aggregations}->{max}->{value_as_string};
        printf "Possible Sessions Days:  %13.2f\n", (0.95*$diskTotal)/($sessionsTotalBytes/$days) if ($days > 0);

        if (exists $ilm->{molochsessions} && exists $ilm->{molochsessions}->{policy}->{phases}->{delete}) {
            printf "ILM Delete Age:      %17s\n", $ilm->{molochsessions}->{policy}->{phases}->{delete}->{min_age};
        }
    }
    printf "History Indices:     %17s\n", commify(scalar(@historys));
    printf "Histories:           %17s (%s bytes)\n", commify($historys), commify($historysBytes);
    if (scalar(@historys) > 0 && $dataNodes > 0) {
        printf "History Density:     %17s (%s bytes)\n", commify(int($historys/($dataNodes*scalar(@historys)))),
                                                       commify(int($historysBytes/($dataNodes*scalar(@historys))));
    }
    printIndex($status, "stats_v30");
    printIndex($status, "stats_v4");
    printIndex($status, "stats_v3");

    printIndex($status, "fields_v30");
    printIndex($status, "fields_v3");
    printIndex($status, "fields_v2");

    printIndex($status, "files_v30");
    printIndex($status, "files_v6");
    printIndex($status, "files_v5");

    printIndex($status, "users_v30");
    printIndex($status, "users_v7");
    printIndex($status, "users_v6");
    printIndex($status, "users_v5");
    printIndex($status, "users_v4");

    printIndex($status, "hunts_v30");
    printIndex($status, "hunts_v2");
    printIndex($status, "hunts_v1");

    printIndex($status, "dstats_v30");
    printIndex($status, "dstats_v4");
    printIndex($status, "dstats_v3");

    printIndex($status, "sequence_v30");
    printIndex($status, "sequence_v3");
    printIndex($status, "sequence_v2");
    exit 0;
} elsif ($ARGV[1] eq "mv") {
    (my $fn = $ARGV[2]) =~ s/\//\\\//g;
    my $results = esGet("/${PREFIX}files/_search?q=name:$fn");
    die "Couldn't find '$ARGV[2]' in db\n" if (@{$results->{hits}->{hits}} == 0);

    foreach my $hit (@{$results->{hits}->{hits}}) {
        my $script = '{"script" : "ctx._source.name = \"' . $ARGV[3] . '\"; ctx._source.locked = 1;"}';
        esPost("/${PREFIX}files/_update/" . $hit->{_id}, $script);
    }
    logmsg "Moved " . scalar (@{$results->{hits}->{hits}}) . " file(s) in database\n";
    exit 0;
} elsif ($ARGV[1] eq "rm") {
    (my $fn = $ARGV[2]) =~ s/\//\\\//g;
    my $results = esGet("/${PREFIX}files/_search?q=name:$fn");
    die "Couldn't find '$ARGV[2]' in db\n" if (@{$results->{hits}->{hits}} == 0);

    foreach my $hit (@{$results->{hits}->{hits}}) {
        esDelete("/${PREFIX}files/_doc/" . $hit->{_id}, 0);
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
            esDelete("/${PREFIX}files/_doc/" . $hit->{_id}, 0);
        }
    }
    exit 0;
} elsif ($ARGV[1] =~ /^rm-?node$/) {
    esGet("/${PREFIX}files,${PREFIX}dstats,${PREFIX}stats/_refresh", 1);
    my $results;

    $results = esGet("/${PREFIX}files/_search?size=1000&q=node:$ARGV[2]&rest_total_hits_as_int=true");
    logmsg "Deleting ", $results->{hits}->{total}, " files\n";
    while ($results->{hits}->{total} > 0) {
        foreach my $hit (@{$results->{hits}->{hits}}) {
            esDelete("/${PREFIX}files/_doc/" . $hit->{_id}, 0);
        }
        esPost("/_flush/synced", "", 1);
        esGet("/${PREFIX}files/_refresh", 1);
        $results = esGet("/${PREFIX}files/_search?size=1000&q=node:$ARGV[2]&rest_total_hits_as_int=true");
    }

    esDelete("/${PREFIX}stats/_doc/" . $ARGV[2], 1);

    $results = esGet("/${PREFIX}dstats/_search?size=1000&q=nodeName:$ARGV[2]&rest_total_hits_as_int=true");
    logmsg "Deleting ", $results->{hits}->{total}, " stats\n";
    while ($results->{hits}->{total} > 0) {
        foreach my $hit (@{$results->{hits}->{hits}}) {
            esDelete("/${PREFIX}dstats/_doc/" . $hit->{_id}, 0);
        }
        esPost("/_flush/synced", "", 1);
        esGet("/${PREFIX}dstats/_refresh", 1);
        $results = esGet("/${PREFIX}dstats/_search?size=1000&q=nodeName:$ARGV[2]&rest_total_hits_as_int=true");
    }
    exit 0;
} elsif ($ARGV[1] =~ /^hide-?node$/) {
    my $results = esGet("/${PREFIX}stats/stat/$ARGV[2]", 1);
    die "Node $ARGV[2] not found" if (!$results->{found});
    esPost("/${PREFIX}stats/_update/$ARGV[2]", '{"doc": {"hide": true}}');
    exit 0;
} elsif ($ARGV[1] =~ /^unhide-?node$/) {
    my $results = esGet("/${PREFIX}stats/stat/$ARGV[2]", 1);
    die "Node $ARGV[2] not found" if (!$results->{found});
    esPost("/${PREFIX}stats/_update/$ARGV[2]", '{"script" : "ctx._source.remove(\"hide\")"}');
    exit 0;
} elsif ($ARGV[1] =~ /^add-?alias$/) {
    my $results = esGet("/${PREFIX}stats/stat/$ARGV[2]", 1);
    die "Node $ARGV[2] already exists, must remove first" if ($results->{found});
    esPost("/${PREFIX}stats/_doc/$ARGV[2]", '{"nodeName": "' . $ARGV[2] . '", "hostname": "' . $ARGV[3] . '", "hide": true}');
    exit 0;
} elsif ($ARGV[1] =~ /^add-?missing$/) {
    my $dir = $ARGV[3];
    chop $dir if (substr($dir, -1) eq "/");
    opendir(my $dh, $dir) || die "Can't opendir $dir: $!";
    my @files = grep { m/^$ARGV[2]-/ && -f "$dir/$_" } readdir($dh);
    closedir $dh;
    logmsg "Checking ", scalar @files, " files, this may take a while.\n";
    foreach my $file (@files) {
        next if ($file !~ /(\d+)-(\d+).(pcap|arkime)/);
        my $filenum = int($2);
        my $ctime = (stat("$dir/$file"))[10];
        my $info = esGet("/${PREFIX}files/_doc/$ARGV[2]-$filenum", 1);
        if (!$info->{found}) {
            logmsg "Adding $dir/$file $filenum $ctime\n";
            esPost("/${PREFIX}files/_doc/$ARGV[2]-$filenum", to_json({
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
    my $remotefiles = esScroll("files", "", to_json({'query' => {'terms' => {'node' => \@nodes}}}));
    logmsg("\n") if ($verbose > 0);
    my %remotefileshash;
    foreach my $hit (@{$remotefiles}) {
        if (! -f $hit->{_source}->{name}) {
            progress("Removing " . $hit->{_source}->{name} . " id: " . $hit->{_id} . "\n");
            esDelete("/${PREFIX}files/_doc/" . $hit->{_id}, 1);
        } else {
            $remotefileshash{$hit->{_source}->{name}} = $hit->{_source};
        }
    }

    # Now see which local are missing
    foreach my $file (@localfiles) {
        my @stat = stat("$file");
        if (!exists $remotefileshash{$file}) {
            next if ($file !~ /\/([^\/]*)-(\d+)-(\d+).(pcap|arkime)/);
            print $file;
            my $node = $1;
            my $filenum = int($3);
            progress("Adding $file $node $filenum $stat[7]\n");
            esPost("/${PREFIX}files/_doc/$node-$filenum", to_json({
                         'locked' => 0,
                         'first' => $stat[10],
                         'num' => $filenum,
                         'name' => "$file",
                         'node' => $node,
                         'filesize' => $stat[7]}), 1);
        } elsif ($stat[7] != $remotefileshash{$file}->{filesize}) {
            progress("Updating filesize $file $stat[7]\n");
            next if ($file !~ /\/([^\/]*)-(\d+)-(\d+).(pcap|arkime)/);
            my $node = $1;
            my $filenum = int($3);
            $remotefileshash{$file}->{filesize} = $stat[7];
            esPost("/${PREFIX}files/_doc/$node-$filenum", to_json($remotefileshash{$file}), 1);
        }
    }
    logmsg("\n") if ($verbose > 0);
    exit 0;
} elsif ($ARGV[1] =~ /^(field)$/) {
    my $result = esGet("/${PREFIX}fields/_doc/$ARGV[3]", 1);
    my $found = $result->{found};
    die "Field $ARGV[3] isn't found" if (!$found);

    esPost("/${PREFIX}fields/_update/$ARGV[3]", "{\"doc\":{\"disabled\":" . ($ARGV[2] eq "disable"?"true":"false").  "}}");
    exit 0;
} elsif ($ARGV[1] =~ /^force-?put-?version$/) {
    die "This command doesn't work anymore, use force-sessions3-update";
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
} elsif ($ARGV[1] =~ /^ilm$/) {
    parseArgs(4);
    my $forceTime = $ARGV[2];
    die "force time must be num followed by h or d" if ($forceTime !~ /^\d+[hd]/);
    my $deleteTime = $ARGV[3];
    die "delete time must be num followed by h or d" if ($deleteTime !~ /^\d+[hd]/);
    $REPLICAS = 0 if ($REPLICAS == -1);
    $HISTORY = $HISTORY * 7;

    print "Creating history ilm policy '${PREFIX}molochhistory' with: deleteTime ${HISTORY}d\n";
    print "Creating sessions ilm policy '${PREFIX}molochsessions' with: forceTime: $forceTime deleteTime: $deleteTime segments: $SEGMENTS replicas: $REPLICAS\n";
    print "You will need to run db.pl upgrade with --ilm to update the templates the first time you turn ilm on.\n";
    sleep 5;
    my $hpolicy =
qq/ {
  "policy": {
    "phases": {
      "delete": {
        "min_age": "${HISTORY}d",
        "actions": {
          "delete": {}
        }
      }
    }
  }
}/;
    esPut("/_ilm/policy/${PREFIX}molochhistory?master_timeout=${ESTIMEOUT}s", $hpolicy);
    esPut("/${PREFIX}history_v*/_settings?master_timeout=${ESTIMEOUT}s", qq/{"settings": {"index.lifecycle.name": "${PREFIX}molochhistory"}}/, 1);
    print "History Policy:\n$hpolicy\n" if ($verbose > 1);
    sleep 5;

    my $policy;
    if ($DOHOTWARM) {
        $policy =
qq/ {
  "policy": {
    "phases": {
      "hot": {
        "min_age": "0ms",
        "actions": {
          "set_priority": {
            "priority": 95
          }
        }
      },
      "warm": {
        "min_age": "$forceTime",
        "actions": {
          "allocate": {
            "number_of_replicas": $REPLICAS,
            "require": {
              "molochtype": "warm"
            }
          },
          "forcemerge": {
            "max_num_segments": $SEGMENTS
          },
          "set_priority": {
            "priority": 10
          }
        }
      },
      "delete": {
        "min_age": "$deleteTime",
        "actions": {
          "delete": {}
        }
      }
    }
  }
}/;
    } else {
        $policy =
qq/ {
  "policy": {
    "phases": {
      "warm": {
        "min_age": "$forceTime",
        "actions": {
          "allocate": {
            "number_of_replicas": $REPLICAS
          },
          "forcemerge": {
            "max_num_segments": $SEGMENTS
          },
          "set_priority": {
            "priority": 10
          }
        }
      },
      "delete": {
        "min_age": "$deleteTime",
        "actions": {
          "delete": {}
        }
      }
    }
  }
}/;
    }
    esPut("/_ilm/policy/${PREFIX}molochsessions?master_timeout=${ESTIMEOUT}s", $policy);
    esPut("/${OLDPREFIX}sessions2-*,${PREFIX}sessions3-*/_settings?allow_no_indices=true&master_timeout=${ESTIMEOUT}s", qq/{"settings": {"index.lifecycle.name": "${PREFIX}molochsessions"}}/, 1);
    print "Policy:\n$policy\n" if ($verbose > 1);
    exit 0;
} elsif ($ARGV[1] =~ /^reindex$/) {
    my ($src, $dst);

    my $MODE = 0;
    if ($ARGV[-1] eq "--nopcap") {
        $MODE = 1;
        pop @ARGV;
    } elsif ($ARGV[-1] eq "--nopacketlen") {
        $MODE = 2;
        pop @ARGV;
    }

    if (scalar @ARGV == 3) {
        $src = $ARGV[2];
        if ($src =~ /\*/) {
            showHelp("Can not have an * in src index name without supplying a dst index name");
        }

        if ($src =~ /-shrink/) {
            $dst = $src =~ s/-shrink//rg
        } elsif ($src =~ /-reindex/) {
            $dst = $src =~ s/-reindex//rg
        } else {
            $dst = "$src-reindex";
        }
        print "src: $src dst: $dst\n";
    } elsif (scalar @ARGV == 4) {
        $src = $ARGV[2];
        $dst = $ARGV[3];
    } else {
        showHelp("Invalid arguments");
    }

    my $query = {"source" => {"index" => $src}, "dest" => {"index" => $dst}, "conflicts" => "proceed"};
    if ($MODE == 1) {
        $query->{script} = {"source" => 'ctx._source.remove("packetPos");ctx._source.remove("packetLen");ctx._source.remove("fileId");'};
    } elsif ($MODE == 2) {
        $query->{script} = {"source" => 'ctx._source.remove("packetLen");'};
    }

    my $result = esPost("/_reindex?wait_for_completion=false&slices=auto", to_json($query));
    die Dumper($result) if (! exists $result->{task});
    my $task = $result->{task};
    print "task: $task\n";
    sleep 10;

    my $srcCount = esGet("/$src/_count")->{count};
    my $dstCount = esGet("/$dst/_count")->{count};

    my $lastp = -1;
    while (1) {
        $result = esGet("/_tasks/$task");
        $dstCount = esGet("/$dst/_count")->{count};
        die Dumper($result->{error}) if (exists $result->{error});
        my $p = int($dstCount * 100 / $srcCount);
        if ($lastp != $p) {
            print (scalar localtime() . " $p% ($dstCount/$srcCount)\n");
            $lastp = $p;
        }
        last if ($result->{completed});
        sleep 30;
    }
    esGet("/${dst}/_flush", 1);
    esGet("/${dst}/_refresh", 1);
    $srcCount = esGet("/$src/_count")->{count};
    $dstCount = esGet("/$dst/_count")->{count};
    die "Mismatch counts $srcCount != $dstCount" if ($srcCount != $dstCount);
    die "Not deleting src since would delete dst too" if ("${dst}*" eq "$src");
    esDelete("/$src", 1);
    print "Deleted $src\n";
    exit 0;
} elsif ($ARGV[1] =~ /^repair$/) {
    dbVersion(1);
    if ($main::versionNumber != $VERSION) {
        die "Must upgrade before trying to do a repair";
    }

    my $indicesa = esGet("/_cat/indices/${PREFIX}*?format=json", 1);
    my %indices = map { $_->{index} => $_ } @{$indicesa};

    foreach my $i ("sequence_v30", "files_v30") {
        if (!defined $indices{"${PREFIX}$i"}) {
            die "--> Couldn't find index ${PREFIX}$i, can not repair\n"
        }
    }

    foreach my $i ("stats_v30", "dstats_v30", "fields_v30", "queries_v30", "hunts_v30", "lookups_v30", "users_v30", "notifiers_v40", "views_v40") {
        if (!defined $indices{"${PREFIX}$i"}) {
            print "--> Couldn't find index ${PREFIX}$i, repair might fail\n"
        }
    }

    foreach my $i ("stats", "dstats", "fields") {
        if (defined $indices{"${PREFIX}$i"}) {
            print "--> Will delete the index ${PREFIX}$i and recreate as alias\n"
        }
    }

    foreach my $i ("queries", "hunts", "lookups", "users", "notifiers", "views") {
        if (defined $indices{"${PREFIX}$i"}) {
            print "--> Will delete the index ${PREFIX}$i and recreate as alias, this WILL cause data loss in those indices, maybe cancel and run backup first\n"
        }
    }

    waitFor("REPAIR", "Do you want to try and repair your install?");
    $verbose = 3 if ($verbose < 3);

    print "Deleting any indices that should be aliases\n";
    foreach my $i ("stats", "dstats", "fields", "queries", "hunts", "lookups", "users", "notifiers", "views") {
        esDelete("/${PREFIX}$i", 0) if (defined $indices{"${PREFIX}$i"});
    }

    print "Re-adding aliases\n";
    esAlias("add", "sequence_v30", "sequence");
    esAlias("add", "files_v30", "files");
    esAlias("add", "stats_v30", "stats");
    esAlias("add", "dstats_v30", "dstats");
    esAlias("add", "fields_v30", "fields");
    esAlias("add", "queries_v30", "queries");
    esAlias("add", "hunts_v30", "hunts");
    esAlias("add", "lookups_v30", "lookups");
    esAlias("add", "users_v30", "users");
    esAlias("add", "notifiers_v40", "notifiers");
    esAlias("add", "views_v40", "views");

    if (defined $indices{"${PREFIX}users_v30"}) {
        usersUpdate();
    } else {
        usersCreate();
    }

    if (defined $indices{"${PREFIX}fields_v30"}) {
        fieldsUpdate();
    } else {
        fieldsCreate();
    }

    if (defined $indices{"${PREFIX}hunts_v30"}) {
        huntsUpdate();
    } else {
        huntsCreate();
    }

    if (defined $indices{"${PREFIX}stats_v30"}) {
        statsUpdate();
    } else {
        statsCreate();
    }

    if (defined $indices{"${PREFIX}dstats_v30"}) {
        dstatsUpdate();
    } else {
        dstatsCreate();
    }

    if (defined $indices{"${PREFIX}lookups_v30"}) {
        lookupsUpdate();
    } else {
        lookupsCreate();
    }

    if (defined $indices{"${PREFIX}queries_v30"}) {
        queriesUpdate();
    } else {
        queriesCreate();
    }

    if (defined $indices{"${PREFIX}notifiers_v40"}) {
        notifiersUpdate();
    } else {
        notifiersCreate();
    }

    if (defined $indices{"${PREFIX}views_v40"}) {
        viewsUpdate();
    } else {
        viewsCreate();
    }

    print "\n";
    print "* You should also run ./db.pl update again\n";
    print "* If having fields issues make sure you restart 1 capture after running repair to see if it fixes\n";

    exit 0;
}

sub dataNodes
{
my ($nodes) = @_;
    my $total = 0;

    foreach my $key (keys %{$nodes}) {
        $total++ if ('data' ~~ @{$nodes->{$key}->{roles}});
    }
    return $total;
}


my $health = dbCheckHealth();

my $nodes = esGet("/_nodes");
$main::numberOfNodes = dataNodes($nodes->{nodes});
logmsg "It is STRONGLY recommended that you stop ALL Arkime captures and viewers before proceeding.  Use 'db.pl ${main::elasticsearch} backup' to backup db first.\n\n";
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

dbCheck();

dbVersion(1);

if ($ARGV[1] eq "wipe" && $main::versionNumber != $VERSION) {
    die "Can only use wipe if schema is up to date.  Use upgrade first.";
}

if ($ARGV[1] =~ /^(init|wipe|clean)/) {

    if ($ARGV[1] eq "init" && $main::versionNumber >= 0) {
        logmsg "It appears this elastic search cluster already has Arkime installed (version $main::versionNumber), this will delete ALL data in elastic search! (It does not delete the pcap files on disk.)\n\n";
        waitFor("INIT", "do you want to erase everything?");
    } elsif ($ARGV[1] eq "wipe") {
        logmsg "This will delete ALL session data in elastic search! (It does not delete the pcap files on disk or user info.)\n\n";
        waitFor("WIPE", "do you want to wipe everything?");
    } elsif ($ARGV[1] eq "clean") {
        waitFor("CLEAN", "do you want to clean everything?");
    }
    logmsg "Erasing\n";
    esDelete("/${PREFIX}sequence_v30,${OLDPREFIX}sequence_v3,${OLDPREFIX}sequence_v2,${OLDPREFIX}sequence_v1,${OLDPREFIX}sequence,${PREFIX}sequence?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}files_v30,${OLDPREFIX}files_v6,${OLDPREFIX}files_v5,${OLDPREFIX}files_v4,${OLDPREFIX}files_v3,${OLDPREFIX}files,${PREFIX}files?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}stats_v30,${OLDPREFIX}stats_v4,${OLDPREFIX}stats_v3,${OLDPREFIX}stats_v2,${OLDPREFIX}stats_v1,${OLDPREFIX}stats,${PREFIX}stats?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}dstats_v30,${OLDPREFIX}dstats_v4,${OLDPREFIX}dstats_v3,${OLDPREFIX}dstats_v2,${OLDPREFIX}dstats_v1,${OLDPREFIX}dstats,${PREFIX}dstats?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}fields_v30,${OLDPREFIX}fields_v3,${OLDPREFIX}fields_v2,${OLDPREFIX}fields_v1,${OLDPREFIX}fields,${PREFIX}fields?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}hunts_v30,${OLDPREFIX}hunts_v2,${OLDPREFIX}hunts_v1,${OLDPREFIX}hunts,${PREFIX}hunts?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}lookups_v30,${OLDPREFIX}lookups_v1,${OLDPREFIX}lookups,${PREFIX}lookups?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}notifiers_v40,${PREFIX}notifiers?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}views_v40,${PREFIX}views?ignore_unavailable=true", 1);
    my $indices;
    esDelete("/$indices" , 1) if (($indices = esMatchingIndices("${OLDPREFIX}sessions2-*")) ne "");
    esDelete("/$indices" , 1) if (($indices = esMatchingIndices("${PREFIX}sessions3-*")) ne "");
    esDelete("/$indices" , 1) if (($indices = esMatchingIndices("${OLDPREFIX}history_v1-*")) ne "");
    esDelete("/$indices" , 1) if (($indices = esMatchingIndices("${PREFIX}history_v1-*")) ne "");
    esDelete("/_template/${OLDPREFIX}template_1", 1);
    esDelete("/_template/${OLDPREFIX}sessions_template", 1);
    esDelete("/_template/${OLDPREFIX}sessions2_template", 1);
    esDelete("/_template/${PREFIX}sessions3_template", 1);
    esDelete("/_template/${PREFIX}sessions3_ecs_template", 1);
    esDelete("/_template/${OLDPREFIX}history_v1_template", 1);
    esDelete("/_template/${PREFIX}history_v1_template", 1);
    if ($ARGV[1] =~ /^(init|clean)/) {
        esDelete("/${PREFIX}users_v30,${OLDPREFIX}users_v7,${OLDPREFIX}users_v6,${OLDPREFIX}users_v5,${OLDPREFIX}users,${PREFIX}users?ignore_unavailable=true", 1);
        esDelete("/${PREFIX}queries_v30,${OLDPREFIX}queries_v3,${OLDPREFIX}queries_v2,${OLDPREFIX}queries_v1,${OLDPREFIX}queries,${PREFIX}queries?ignore_unavailable=true", 1);
    }
    esDelete("/tagger", 1);

    sleep(1);

    exit 0 if ($ARGV[1] =~ "clean");

    logmsg "Creating\n";
    sequenceCreate();
    filesCreate();
    statsCreate();
    dstatsCreate();
    sessions3Update();
    fieldsCreate();
    historyUpdate();
    huntsCreate();
    lookupsCreate();
    notifiersCreate();
    viewsCreate();
    if ($ARGV[1] =~ "init") {
        usersCreate();
        queriesCreate();
    }
} elsif ($ARGV[1] =~ /^restore$/) {

    logmsg "It is STRONGLY recommended that you stop ALL Arkime captures and viewers before proceeding.\n";

    dbCheckForActivity($PREFIX);

    my @indexes = ("users", "sequence", "stats", "queries", "hunts", "files", "fields", "dstats", "lookups", "notifiers", "views");
    my @filelist = ();
    foreach my $index (@indexes) { # list of data, settings, and mappings files
        push(@filelist, "$ARGV[2].${PREFIX}${index}.json\n") if (-e "$ARGV[2].${PREFIX}${index}.json");
        push(@filelist, "$ARGV[2].${PREFIX}${index}.settings.json\n") if (-e "$ARGV[2].${PREFIX}${index}.settings.json");
        push(@filelist, "$ARGV[2].${PREFIX}${index}.mappings.json\n") if (-e "$ARGV[2].${PREFIX}${index}.mappings.json");
    }
    foreach my $index ("sessions2", "sessions3", "history") { # list of templates
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

    esDelete("/${PREFIX}sequence_v30,${OLDPREFIX}sequence_v3,${OLDPREFIX}sequence_v2,${OLDPREFIX}sequence_v1,${OLDPREFIX}sequence,${PREFIX}sequence?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}files_v30,${OLDPREFIX}files_v6,${OLDPREFIX}files_v5,${OLDPREFIX}files_v4,${OLDPREFIX}files_v3,${OLDPREFIX}files,${PREFIX}files?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}stats_v30,${OLDPREFIX}stats_v4,${OLDPREFIX}stats_v3,${OLDPREFIX}stats_v2,${OLDPREFIX}stats_v1,${OLDPREFIX}stats,${PREFIX}stats?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}dstats_v30,${OLDPREFIX}dstats_v4,${OLDPREFIX}dstats_v3,${OLDPREFIX}dstats_v2,${OLDPREFIX}dstats_v1,${OLDPREFIX}dstats,${PREFIX}dstats?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}fields_v30,${OLDPREFIX}fields_v3,${OLDPREFIX}fields_v2,${OLDPREFIX}fields_v1,${OLDPREFIX}fields,${PREFIX}fields?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}hunts_v30,${OLDPREFIX}hunts_v2,${OLDPREFIX}hunts_v1,${OLDPREFIX}hunts,${PREFIX}hunts?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}lookups_v30,${OLDPREFIX}lookups_v1,${OLDPREFIX}lookups,${PREFIX}lookups?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}notifiers_v40,${PREFIX}notifiers?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}views_v40,${PREFIX}views?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}users_v30,${OLDPREFIX}users_v7,${OLDPREFIX}users_v6,${OLDPREFIX}users_v5,${OLDPREFIX}users,${PREFIX}users?ignore_unavailable=true", 1);
    esDelete("/${PREFIX}queries_v30,${OLDPREFIX}queries_v3,${OLDPREFIX}queries_v2,${OLDPREFIX}queries_v1,${OLDPREFIX}queries,${PREFIX}queries?ignore_unavailable=true", 1);

    esDelete("/_template/${PREFIX}sessions3_ecs_template", 1);
    esDelete("/_template/${PREFIX}sessions3_template", 1);
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
            esPut("/$index[0]?master_timeout=${ESTIMEOUT}s", $settings);
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
    my @templates = ("sessions2", "sessions3", "history");
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
                my $indices = esGet("/${OLDPREFIX}sessions2-*/_alias", 1);
                logmsg "Updating sessions2 mapping for ", scalar(keys %{$indices}), " indices\n" if (scalar(keys %{$indices}) != 0);
                foreach my $i (keys %{$indices}) {
                    progress("$i ");
                    esPut("/$i/session/_mapping?master_timeout=${ESTIMEOUT}s", to_json($mapping), 1);
                }
                logmsg "\n";
            } elsif (($template cmp "sessions3") == 0 && $UPGRADEALLSESSIONS) {
                my $indices = esGet("/${PREFIX}sessions3-*/_alias", 1);
                logmsg "Updating sessions3 mapping for ", scalar(keys %{$indices}), " indices\n" if (scalar(keys %{$indices}) != 0);
                foreach my $i (keys %{$indices}) {
                    progress("$i ");
                    esPut("/$i/_mapping?master_timeout=${ESTIMEOUT}s", to_json($mapping), 1);
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
    if ($main::versionNumber < 72) {
        logmsg "Can not upgrade directly, please upgrade to Moloch 3.3.0+ first. (Db version $main::versionNumber)\n\n";
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

    if ($main::versionNumber < 75) {
        checkForOld7Indices();
        sessions3Update();
        historyUpdate();
        huntsUpdate();
        lookupsUpdate();
        notifiersCreate();
        notifiersMove();
        viewsCreate();
        viewsMove();
        queriesUpdate();
        usersUpdate();
    } elsif ($main::versionNumber == 75) {
        checkForOld7Indices();
        sessions3Update();
        historyUpdate();
        viewsCreate();
        viewsMove();
        queriesUpdate();
        usersUpdate();
    } elsif ($main::versionNumber <= 77) {
        checkForOld7Indices();
        sessions3Update();
        historyUpdate();
        queriesUpdate();
        usersUpdate();
    } elsif ($main::versionNumber <= 78) {
        checkForOld7Indices();
        sessions3Update();
        historyUpdate();
        queriesUpdate();
    } else {
        logmsg "db.pl is hosed\n";
    }
}

if ($DOHOTWARM) {
    esPut("/${PREFIX}stats_v30,${PREFIX}dstats_v30,${PREFIX}fields_v30,${PREFIX}files_v30,${PREFIX}sequence_v30,${PREFIX}users_v30,${PREFIX}queries_v30,${PREFIX}hunts_v30,${PREFIX}history*,${PREFIX}lookups_v30,${PREFIX}notifiers_v40/_settings?master_timeout=${ESTIMEOUT}s&allow_no_indices=true&ignore_unavailable=true,${PREFIX}views_v40/_settings?master_timeout=${ESTIMEOUT}s&allow_no_indices=true&ignore_unavailable=true", "{\"index.routing.allocation.require.molochtype\": \"warm\"}");
} else {
    esPut("/${PREFIX}stats_v30,${PREFIX}dstats_v30,${PREFIX}fields_v30,${PREFIX}files_v30,${PREFIX}sequence_v30,${PREFIX}users_v30,${PREFIX}queries_v30,${PREFIX}hunts_v30,${PREFIX}history*,${PREFIX}lookups_v30,${PREFIX}notifiers_v40/_settings?master_timeout=${ESTIMEOUT}s&allow_no_indices=true&ignore_unavailable=true,${PREFIX}views_v40/_settings?master_timeout=${ESTIMEOUT}s&allow_no_indices=true&ignore_unavailable=true", "{\"index.routing.allocation.require.molochtype\": null}");
}

logmsg "Finished\n";

sleep 1;
esGet("/_flush", 1);
esGet("/_refresh", 1);
