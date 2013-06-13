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

use HTTP::Request::Common;
use LWP::UserAgent;
use JSON;
use Data::Dumper;
use POSIX;
use strict;

my $VERSION = 10;
my $verbose = 0;

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
    print "$0 [-v] <ESHOST:ESPORT> <command> [<options>]\n";
    print "\n";
    print "  -v                    - Verbose, multiple increases level\n";
    print "\n";
    print "Commands:\n";
    print "  init                  - Clear ALL elasticsearch moloch data and create schema\n";
    print "  wipe                  - Same as init, but leaves user database untouched\n";
    print "  upgrade               - Upgrade Moloch's schema in elasticsearch from previous versions\n";
    print "  info                  - Information about the database\n";
    print "  usersexport <fn>      - Save the users info to <fn>\n";
    print "  usersimport <fn>      - Load the users info from <fn>\n";
    print "  rotate <type> <num>   - Perform daily maintenance\n";
    print "       type             - Same as rotateIndex in ini file = daily,weekly,monthly\n";
    print "       num              - number indexes to keep\n";
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
    print "GET http://$ARGV[0]$url\n" if ($verbose > 2);
    my $response = $main::userAgent->get("http://$ARGV[0]$url");
    if ($response->code == 500 || ($response->code != 200 && !$dontcheck)) {
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
    print "Copying " . $status->{indices}->{$srci}->{docs}->{num_docs} . " elements from $srci/$type to $dsti/$type\n";

    my $id = "";
    while (1) {
        if ($verbose > 0) {
            local $| = 1;
            print ".";
        }
        my $url;
        if ($id eq "") {
            $url = "/$srci/$type/_search?scroll=10m&scroll_id=$id&size=500";
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
    print "\n"
}
################################################################################
sub esAlias
{
    my ($cmd, $index, $alias) = @_;

    print "Alias cmd $cmd from $index to alias $alias\n" if ($verbose > 0);
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

    print "Creating tags_v2 index\n" if ($verbose > 0);
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

    print "Setting tags_v2 mapping\n" if ($verbose > 0);
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

    print "Creating sequence index\n" if ($verbose > 0);
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

    print "Setting sequence mapping\n" if ($verbose > 0);
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

    print "Creating files_v3 index\n" if ($verbose > 0);
    esPut("/files_v3", $settings);
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
    esPut("/files_v3/file/_mapping", $mapping);
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
      },
      memory: {
        type: "long",
        index: "no"
      }
    }
  }
}';

    print "Setting stats mapping\n" if ($verbose > 0);
    esPut("/stats/stat/_mapping?pretty&ignore_conflicts=true", $mapping, 1);
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
      },
      memory: {
        type: "long",
        index: "no"
      }
    }
  }
}';

    print "Setting dstats_v1 mapping\n" if ($verbose > 0);
    esPut("/dstats_v1/dstat/_mapping?pretty&ignore_conflicts=true", $mapping, 1);
}

################################################################################
sub sessionsUpdate
{
    my $mapping = '
{
  session: {
    _all : {enabled : false},
    dynamic: "strict",
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
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          as1: {type: "string", analyzer: "snowball"},
          rawas1: {type: "string", index: "not_analyzed"}
        }
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
        type: "multi_field",
        path: "just_name",
        omit_norms: true,
        fields: {
          as2: {type: "string", analyzer: "snowball"},
          rawas2: {type: "string", index: "not_analyzed"}
        }
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
      hdrs: {
        type: "object",
        dynamic: "true"
      }
    }
  }
}
';

    my $template = '
{
  template: "session*",
  settings: {
    "index.fielddata.cache": "soft",
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

    print "Creating sessions template\n" if ($verbose > 0);
    esPut("/_template/template_1", $template);

    my $status = esGet("/sessions-*/_stats?clear=1", 1);
    my $indices = $status->{indices} || $status->{_all}->{indices};

    print "Updating sessions mapping for ", scalar(keys %{$indices}), " indices\n";
    foreach my $i (keys %{$indices}) {
        if ($verbose == 1) {
            local $| = 1;
            print ".";
        } elsif ($verbose  > 1) {
            print "  Updating sessions mapping for $i\n";
        }
        esPut("/$i/session/_mapping?ignore_conflicts=true", $mapping);
        esPost("/$i/_close", "");
        esPut("/$i/_settings", '{"index.fielddata.cache": "soft"}');
        esPost("/$i/_open", "");
    }

    print "\n";
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

    print "Creating users_v2 index\n" if ($verbose > 0);
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

    print "Setting users_v2 mapping\n" if ($verbose > 0);
    esPut("/users_v2/user/_mapping?pretty&ignore_conflicts=true", $mapping);
}

################################################################################
sub time2index
{
my($type, $t) = @_;

    my @t = localtime($t);
    if ($type eq "daily") {
        return sprintf("sessions-%02d%02d%02d", $t[5] % 100, $t[4]+1, $t[3]);
    } 
    
    if ($type eq "weekly") {
        return sprintf("sessions-%02dw%02d", $t[5] % 100, int($t[7]/7));
    } 
    
    if ($type eq "monthly") {
        return sprintf("sessions-%02dm%02d", $t[5] % 100, $t[4]+1);
    }
}
################################################################################
sub dbVersion {
my ($loud) = @_;

    my $version = esGet("/dstats/version/version", 1);

    if (!exists $version->{exists}) {
        print "This is a fresh Moloch install\n" if ($loud);
        $main::versionNumber = -1;
        if ($loud && $ARGV[1] ne "init") {
            die "Looks like moloch wasn't installed, must do init"
        }
    } elsif ($version->{exists} == 0) {
        $main::versionNumber = 0;
    } else {
        $main::versionNumber = $version->{_source}->{version};
    }
}
################################################################################
while (@ARGV > 0 && substr($ARGV[0], 0, 1) eq "-") {
    if ($ARGV[0] eq "-v") {
        $verbose++;
        shift @ARGV;
    }
}

showHelp("Help:") if ($ARGV[1] =~ /^help$/);
showHelp("Missing arguments") if (@ARGV < 2);
showHelp("Unknown command '$ARGV[1]'") if ($ARGV[1] !~ /^(init|info|wipe|upgrade|usersimport|usersexport|rotate)$/);
showHelp("Missing arguments") if (@ARGV < 3 && $ARGV[1] =~ /^(usersimport|usersexport)/);
showHelp("Must have both <type> and <num> arguments") if (@ARGV < 4 && $ARGV[1] =~ /^(rotate)/);

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
} elsif ($ARGV[1] eq "rotate") {
    showHelp("Invalid rotate <type>") if ($ARGV[2] !~ /^(daily|weekly|monthly)$/);
    my $json = esGet("/sessions-*/_stats?clear=1", 1);
    my $indices = $json->{indices} || $json->{_all}->{indices};

    my $endTime = time();
    my $endTimeIndex = time2index($ARGV[2], $endTime);
    my @startTime = localtime;
    if ($ARGV[2] eq "daily") {
        $startTime[3] -= int($ARGV[3]);
    } elsif ($ARGV[2] eq "weekly") {
        $startTime[3] -= 7*int($ARGV[3]);
    } elsif ($ARGV[2] eq "monthly") {
        $startTime[4] -= int($ARGV[3]);
    }

    my $startTime = mktime(@startTime);
    while ($startTime <= $endTime) {
        my $iname = time2index($ARGV[2], $startTime);
        if (exists $indices->{$iname}) {
            $indices->{$iname}->{OPTIMIZEIT} = 1;
        }
        $startTime += 24*60*60;
    }

    foreach my $i (sort (keys %{$indices})) {
        next if ($endTimeIndex eq $i);
        if (exists $indices->{$i}->{OPTIMIZEIT}) {
            esGet("/$i/_optimize?max_num_segments=4", 1);
        } else {
            esDelete("/$i", 1);
        }
    }
    exit 0;
} elsif ($ARGV[1] eq "info") {
    dbVersion(0);
    my $esversion = esGet("/");
    my $nodes = esGet("/_nodes");
    my $status = esGet("/_status", 1);
    my $sessions = 0;
    my $sessionsBytes = 0;
    my @sessions = grep /^session/, keys %{$status->{indices}};
    foreach my $index (@sessions) {
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
    printf "Nodes:               %s\n", commify(scalar(keys %{$nodes->{nodes}}));
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
    printIndex($status->{indices}->{users_v2}, "users_v2");
    printIndex($status->{indices}->{users_v1}, "users_v1");
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

dbVersion(1);

if ($ARGV[1] eq "wipe" && $main::versionNumber != $VERSION) {
    die "Can only use wipe if schema is up to date.  Use upgrade first.";
}

if ($ARGV[1] =~ /(init|wipe)/) {

    if ($ARGV[1] eq "init" && $main::versionNumber >= 0) {
        print "It appears this elastic search cluster already has moloch installed, this will delete ALL data in elastic search! (It does not delete the pcap files on disk.)\n\n";
        print "Type \"INIT\" to continue - do you want to erase everything?\n";
        waitFor("INIT");
    } elsif ($ARGV[1] eq "wipe") {
        print "This will delete ALL session data in elastic search! (It does not delete the pcap files on disk or user info.)\n\n";
        print "Type \"WIPE\" to continue - do you want to wipe everything?\n";
        waitFor("WIPE");
    }
    print "Erasing\n";
    esDelete("/tags_v2", 1);
    esDelete("/tags", 1);
    esDelete("/sequence", 1);
    esDelete("/files_v3", 1);
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
    print "Type \"UPGRADE\" to continue - do you want to upgrade?\n";
    waitFor("UPGRADE");
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
    esCopy("files_v1", "files_v3", "file");

    esAlias("remove", "users_v1", "users");
    esCopy("users_v1", "users_v2", "user");

    esCopy("dstats", "dstats_v1", "user");
    sleep 1;

    esDelete("/dstats", 1);
    sleep 1;

    esAlias("add", "dstats_v1", "dstats");

    print "users_v1 and files_v1 tables can be deleted now\n";
    print "Finished\n";
} elsif ($main::versionNumber >= 1 && $main::versionNumber < 7) {
    print "Trying to upgrade from version $main::versionNumber to version $VERSION.\n\n";
    print "Type \"UPGRADE\" to continue - do you want to upgrade?\n";
    waitFor("UPGRADE");
    print "Starting Upgrade\n";

    filesCreate();
    esAlias("remove", "files_v2", "files");
    esCopy("files_v2", "files_v3", "file");
    print "files_v2 table can be deleted now\n";

    sessionsUpdate();
    usersUpdate();
    statsUpdate();
    dstatsUpdate();

    print "Finished\n";
} elsif ($main::versionNumber >= 7 && $main::versionNumber <= 10) {
    print "Trying to upgrade from version $main::versionNumber to version $VERSION.\n\n";
    print "Type \"UPGRADE\" to continue - do you want to upgrade?\n";
    waitFor("UPGRADE");
    print "Starting Upgrade\n";

    filesUpdate();
    sessionsUpdate();
    usersUpdate();
    statsUpdate();
    dstatsUpdate();

    print "Finished\n";
} else {
    print "db.pl is hosed\n";
}

sleep 1;
esPost("/dstats/version/version", "{\"version\": $VERSION}");
