#!/usr/bin/perl -w
# Used to upload new tagger files to the database.  The filename is used as
# a database key, so make sure to reuse the same filename.
#
# SPDX-License-Identifier: Apache-2.0

use strict;
use HTTP::Request::Common;
use LWP::UserAgent;
use Digest::MD5 qw(md5_hex);
use Data::Dumper;

my $INSECURE = 0;
if ($ARGV[0] eq "--insecure") {
    $INSECURE = 1;
    shift @ARGV;
}

my $host = $ARGV[0];

if ($host !~ /(http:|https)/) {
    $host = "http://$ARGV[0]";
}

sub showHelp($)
{
    my ($str) = @_;
    print $str,"\n";
    die "$0 [--insecure] ESHOST:ESPORT (ip|host|md5|email|uri) filename tag1 [tag2..tagN]";
}

showHelp("Missing arguments") if (@ARGV < 4);
showHelp("Must be ip, host, or md5 for file type instead of $ARGV[1]") if ($ARGV[1] !~ /^(host|ip|md5|email|uri)$/);
showHelp("file '$ARGV[2]' not found") if (! -f $ARGV[2]);
showHelp("file '$ARGV[2]' empty") if (-z $ARGV[2]);

my $userAgent = LWP::UserAgent->new(timeout => 20);

if ($INSECURE) {
    $userAgent->ssl_opts(
        SSL_verify_mode => 0,
        verify_hostname=> 0
    )
}

my $response = $userAgent->request(HTTP::Request::Common::PUT("$host/tagger",
                               "Content-Type" => "application/json;charset=UTF-8",
                                Content => '{"mappings": { "properties":{"tags":{"type":"keyword","index": false}, "type": {"type":"keyword","index": false}, "data": {"type":"keyword","index": false}, "md5": {"type":"keyword","index": false}}}}'));
#print Dumper($response) if ($response->code != 200);

my @ELEMENTS;
open (FILE, $ARGV[2]);
my $fields = "";
while (<FILE>) {
    if (/^#field:/) {
        chop;
        $fields = '"fields":"' if ($fields eq "");
        $fields .= substr($_, 1) . ",";
        next;
    }
    next if (/^#/ || /^$/);
    chop;
    push(@ELEMENTS, $_);
}

if ($fields ne "") {
    chop $fields;
    $fields .= "\", ";
}
my $elements = join ',', @ELEMENTS;
close (FILE);

my $md5hex = md5_hex($elements);

my $content  = '{' . $fields . '"tags": "' . join(',', @ARGV[3 .. $#ARGV]) . '", "md5":"' . $md5hex .'", "type":"' . $ARGV[1] . '", "data":"' . $elements . '"}'. "\n";
$response = $userAgent->post("$host/tagger/_doc/$ARGV[2]", "Content-Type" => "application/json;charset=UTF-8", Content => $content);
print $response->content, "\n";

