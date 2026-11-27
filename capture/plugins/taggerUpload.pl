#!/usr/bin/perl -w
# Used to upload new tagger files to the database.  The filename is used as
# a database key, so make sure to reuse the same filename.
# Runs taggerUpload.js, which can also use a config file or a dbUrl without OpenSearch/Elasticsearch.
#
# SPDX-License-Identifier: Apache-2.0

use strict;
use Cwd qw(abs_path);
use File::Basename qw(dirname);

sub showHelp($)
{
    my ($str) = @_;
    print $str,"\n";
    die "$0 [--insecure] ESHOST:ESPORT (ip|host|md5|email|uri) filename [tag1..tagN]";
}

my @insecure;
if (@ARGV && $ARGV[0] eq "--insecure") {
    @insecure = ("--insecure");
    shift @ARGV;
}
showHelp("Missing arguments") if (@ARGV < 3);

# Installed this is in bin beside node with the js in plugins, in the source tree it's beside the js
my $dir = dirname(abs_path($0));
my $js = -f "$dir/taggerUpload.js" ? "$dir/taggerUpload.js" : "$dir/../plugins/taggerUpload.js";
my $node = -x "$dir/node" ? "$dir/node" : "node";
my $host = shift @ARGV;

exec($node, $js, @insecure, "--url", $host, @ARGV) or die "Couldn't run $node $js: $!";
