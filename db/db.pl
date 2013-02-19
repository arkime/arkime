#!/usr/bin/perl 
# This script can upgrade or init the elastic search db

use HTTP::Request::Common;
use LWP::UserAgent;
use JSON;
use Data::Dumper;
use strict;

my $VERSION = 4;

################################################################################
sub MIN ($$) { $_[$_[0] > $_[1]] }
sub MAX ($$) { $_[$_[0] < $_[1]] }

################################################################################
sub showHelp($)
{
    my ($str) = @_;
    print "\n", $str,"\n\n";
    print "$0 <ESHOST:ESPORT> <command> [<options>]\n";
    print "\n";
    print "Commands:\n";
    print "  init             - Clear ALL elasticsearch moloch data and create schema\n";
    print "  wipe             - Same as init, but leaves user database untouched\n";
    print "  upgrade          - Upgrade Moloch's schema in elasticsearch from previous versions\n";
    print "  usersexport <fn> - Save the users info to <fn>\n";
    print "  usersimport <fn> - Load the users info from <fn>\n";
    exit 1;
}
################################################################################
sub waitFor
{
    my ($str) = @_;

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
    my $response = $main::userAgent->get("http://$ARGV[0]$url");
    if ($response->code != 200 && !$dontcheck) {
      die "Couldn't get $url with code " . $response->code;
    }
    my $json = from_json($response->content);
    return $json
}

################################################################################
sub esPost
{
    my ($url, $content, $dontcheck) = @_;
    my $response = $main::userAgent->post("http://$ARGV[0]$url", Content => $content);
    if ($response->code != 200 && $response->code != 201 && !$dontcheck) {
      die "Couldn't post $url with code " . $response->code . $response->content;
    }
    my $json = from_json($response->content);
    return $json
}

################################################################################
sub esPut
{
    my ($url, $content, $dontcheck) = @_;
    my $response = $main::userAgent->request(HTTP::Request::Common::PUT("http://$ARGV[0]$url", Content => $content));
    if ($response->code != 200 && !$dontcheck) {
      die "Couldn't put $url with code " . $response->code . $response->content;
    }
    my $json = from_json($response->content);
    return $json
}

################################################################################
sub esDelete
{
    my ($url, $dontcheck) = @_;
    my $response = $main::userAgent->request(HTTP::Request::Common::_simple_req("DELETE", "http://$ARGV[0]$url"));
    if ($response->code != 200 && !$dontcheck) {
      die "Couldn't delete $url with code " . $response->code;
    }
    my $json = from_json($response->content);
    return $json
}

################################################################################
sub esCopy
{
    my ($srci, $dsti, $type) = @_;
    my $id = "";
    while (1) {
        my $url;
        if ($id eq "") {
            $url = "/$srci/$type/_search?scroll=10m&scroll_id=$id&size=100";
        } else {
            $url = "/_search/scroll?scroll=10m&scroll_id=$id";
        }
        

        my $incoming = esGet($url);
        my $out = "";
        last if (@{$incoming->{hits}->{hits}} == 0);

        foreach my $hit (@{$incoming->{hits}->{hits}}) {
            $out .= "{\"index\": {\"_index\": \"$dsti\", \"_type\": \"$type\", \"_id\": \"" . $hit->{_id} . "\"}}\n";
            $out .= to_json($hit->{_source}) . "\n";
        }

        $id = $incoming->{_scroll_id};

        esPost("/_bulk", $out);
    }
}
################################################################################
sub esAlias
{
    my ($cmd, $index, $alias) = @_;
    esPost("/_aliases", '{ actions: [ { ' . $cmd . ': { index: "' . $index . '", alias : "'. $alias .'" } } ] }', 1);
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

    esPut("/tags_v2/", $settings);
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

    esPut("/tags_v2/tag/_mapping", $mapping);
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

    esPut("/sequence", $settings);
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

    esPut("/sequence/sequence/_mapping", $mapping);
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

    esPut("/files_v2", $settings);
    esAlias("add", "files_v2", "files");
    filesUpdate();
}
################################################################################
sub filesUpdate
{
    my $mapping = '
{
  file: {
    _all : {"enabled" : false},
    _source : {"enabled" : true},
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
        index: "no"
      },
      size: {
        type: "long",
        index: "no"
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

    esPut("/files_v2/file/_mapping", $mapping);
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

    esPut("/stats", $settings);
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
      }
    }
  }
}';

    esPut("/stats/stat/_mapping", $mapping, 1);
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

    esPut("/dstats_v1", $settings);
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
      }
    }
  }
}';

    esPut("/dstats_v1/dstat/_mapping", $mapping, 1);
}

################################################################################
sub sessionsUpdate
{
    my $mapping = '
{
  session: {
    _all : {enabled : false},
    dynamic: "strict",
    properties: {
      us: {
        omit_norms: true,
        type: "string",
        analyzer: "url_analyzer"
      },
      uscnt: {
        type: "integer"
      },
      ua: {
        omit_norms: true,
        type: "string",
        analyzer: "url_analyzer"
      },
      uacnt: {
        type: "integer"
      },
      ps: {
        type: "long",
        index: "no"
      },
      fs: {
        type: "long"
      },
      lp: {
        type: "long"
      },
      lpms: {
        type: "short"
      },
      fp: {
        type: "long"
      },
      fpms: {
        type: "short"
      },
      a1: {
        type: "long"
      },
      g1: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      as1: {
        omit_norms: true,
        type: "string",
        analyzer: "snowball"
      },
      p1: {
        type: "integer"
      },
      a2: {
        type: "long"
      },
      g2: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      as2: {
        omit_norms: true,
        type: "string",
        analyzer: "snowball"
      },
      xff: {
        type: "long"
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
        omit_norms: true,
        type: "string",
        analyzer: "snowball"
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
        omit_norms: true,
        type: "string",
        analyzer: "snowball"
      },
      p2: {
        type: "integer"
      },
      pr: {
        type: "short"
      },
      pa: {
        type: "integer"
      },
      by: {
        type: "long"
      },
      db: {
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
            omit_norms: true,
            type : "string",
            analyzer : "snowball"
          },
          sCn : {
            omit_norms: true,
            type : "string",
            index : "not_analyzed"
          },
          sOn : {
            omit_norms: true,
            type : "string",
            analyzer : "snowball"
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
        omit_norms: true,
        type : "string",
        analyzer : "snowball"
      },
      esubcnt: {
        type: "short"
      },
      esub : {
        omit_norms: true,
        type : "string",
        analyzer : "snowball"
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
      geip: {
        omit_norms: true,
        type: "string",
        index: "not_analyzed"
      },
      aseip: {
        omit_norms: true,
        type: "string",
        analyzer: "snowball"
      }
    }
  }
}
';

    my $template = '
{
  template: "session*",
  settings: {
    "index.cache.field.type": "soft",
    index: {
      "routing.allocation.total_shards_per_node": 1,
      refresh_interval: 60,
      number_of_shards: ' . $main::numberOfNodes . ',
      number_of_replicas: 0,
      store: { compress: { stored : true, tv: true } },
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

    esPut("/_template/template_1", $template);

    my $status = esGet("/sessions-*/_status", 1);
    foreach my $i (keys %{$status->{indices}}) {
        esPut("/$i/session/_mapping?ignore_conflicts=true", $mapping);
    }
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

    esPut("/users_v2", $settings);
    esAlias("add", "users_v2", "users");
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
        type: "string"
      },
      enabled: {
        type: "boolean",
        index: "no"
      },
      createEnabled: {
        type: "boolean",
        index: "no"
      },
      webEnabled: {
        type: "boolean",
        index: "no"
      },
      headerAuthEnabled: {
        type: "boolean",
        index: "no"
      },
      emailSearch: {
        type: "boolean",
        index: "no"
      },
      passStore: {
        type: "string",
        index: "no"
      },
      expression: {
        type: "string",
        index: "no"
      }
    }
  }
}';

    esPut("/users_v2/user/_mapping?pretty&ignore_conflicts=true", $mapping);
}

################################################################################
showHelp("Help:") if ($ARGV[1] =~ /^help$/);
showHelp("Missing arguments") if (@ARGV < 2);
showHelp("Unknown command '$ARGV[1]'") if ($ARGV[1] !~ /^(init|wipe|upgrade|usersimport|usersexport)$/);
showHelp("Missing arguments") if (@ARGV < 3 && $ARGV[1] =~ /(usersimport|usersexport)/);

$main::userAgent = LWP::UserAgent->new(timeout => 10);

if ($ARGV[1] eq "usersimport") {
    open(my $fh, "<", $ARGV[2]) or die "cannot open < $ARGV[2]: $!";
    my $data = do { local $/; <$fh> };
    esPost("/_bulk", $data);
    close($fh);
    exit 0;
} elsif ($ARGV[1] eq "usersexport") {
    open(my $fh, ">", $ARGV[2]) or die "cannot open > $ARGV[2]: $!";
    my $users = esGet("/users/_search?size=1000");
    foreach my $hit (@{$users->{hits}->{hits}}) {
        print $fh "{\"index\": {\"_index\": \"users\", \"_type\": \"user\", \"_id\": \"" . $hit->{_id} . "\"}}\n";
        print $fh to_json($hit->{_source}) . "\n";
    }
    close($fh);
    exit 0;
}



my $nodes = esGet("/_nodes");
$main::numberOfNodes = keys %{$nodes->{nodes}};
print "It is STRONGLY recommended that you stop ALL moloch captures and viewers before proceeding.\n\n";
if ($main::numberOfNodes == 1) {
    print "There is $main::numberOfNodes elastic search node, if you expect more please fix first before proceeding.\n\n";
} else {
    print "There are $main::numberOfNodes elastic search nodes, if you expect more please fix first before proceeding.\n\n";
}


my $version = esGet("/dstats/version/version", 1);

if (!exists $version->{exists}) {
    print "This is a fresh Moloch install\n";
    $main::versionNumber = -1;
    if ($ARGV[1] ne "init") {
        die "Looks like moloch wasn't installed, must do init"
    }
} elsif ($version->{exists} == 0) {
    $main::versionNumber = 0;
} else {
    $main::versionNumber = $version->{_source}->{version};
}

if ($ARGV[1] eq "wipe" && $main::versionNumber != $VERSION) {
    die "Can only use wipe if schema is up to date.  Use upgrade first.";
}

if ($ARGV[1] =~ /(init|wipe)/) {

    if ($ARGV[1] eq "init" && $main::versionNumber >= 0) {
        print "It appears this elastic search cluster already has moloch installed, this will delete ALL data in elastic search! (It does not delete the pcap files on disk.)\n\n";
        print "Type \"YES\" to continue - do you want to erase everything?\n";
        waitFor("YES");
    } elsif ($ARGV[1] eq "wipe") {
        print "This will delete ALL session data in elastic search! (It does not delete the pcap files on disk or user info.)\n\n";
        print "Type \"YES\" to continue - do you want to wipe everything?\n";
        waitFor("YES");
    }
    print "Erasing\n";
    esDelete("/tags_v2", 1);
    esDelete("/tags", 1);
    esDelete("/sequence", 1);
    esDelete("/files_v2", 1);
    esDelete("/files_v1", 1);
    esDelete("/files", 1);
    esDelete("/stats", 1);
    esDelete("/dstats", 1);
    esDelete("/dstats_v1", 1);
    esDelete("/sessions*", 1);
    esDelete("/template_1", 1);
    if ($ARGV[1] eq "init") {
        esDelete("/users_v1", 1);
        esDelete("/users_v2", 1);
        esDelete("/users", 1);
    }
    esDelete("/tagger", 1);

    sleep(1);

    print "Creating\n";
    tagsCreate();
    sequenceCreate();
    filesCreate();
    statsCreate();
    dstatsCreate();
    sessionsUpdate();
    if ($ARGV[1] eq "init") {
        usersCreate();
    }
    print "Finished.  Have fun!\n";
} elsif ($main::versionNumber == 0) {
    print "Trying to upgrade from version 0 to version $VERSION.  This may or may not work since the elasticsearch moloch db was a wildwest before version 1.  This upgrade will reset some of the stats, sorry.\n\n";
    print "Type \"YES\" to continue - do you want to upgrade?\n";
    waitFor("YES");
    print "Starting Upgrade\n";

    esDelete("/stats", 1);

    tagsUpdate();
    sequenceUpdate();
    filesCreate();
    statsCreate();
    dstatsCreate();
    sessionsUpdate();
    usersCreate();

    esAlias("remove", "files_v1", "files");
    esCopy("files_v1", "files_v2", "file");

    esAlias("remove", "users_v1", "users");
    esCopy("users_v1", "users_v2", "user");

    esCopy("dstats", "dstats_v1", "user");
    sleep 1;

    esDelete("/dstats", 1);
    sleep 1;

    esAlias("add", "dstats_v1", "dstats");

    print "users_v1 and files_v1 tables can be deleted now\n";
    print "Finished\n";
} elsif ($main::versionNumber >= 1 && $main::versionNumber <= 4) {
    print "Trying to upgrade from version $main::versionNumber to version $VERSION.\n\n";
    print "Type \"YES\" to continue - do you want to upgrade?\n";
    waitFor("YES");
    print "Starting Upgrade\n";
    sessionsUpdate();
    usersUpdate();
} else {
    print "db.pl is hosed\n";
}

sleep 1;
esPost("/dstats/version/version", "{\"version\": $VERSION}");
