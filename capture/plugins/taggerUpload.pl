#!/usr/bin/perl -w
# Used to upload new tagger files to the database.  The filename is used as
# a database key, so make sure to reuse the same filename.

use strict;
use HTTP::Request::Common;
use LWP::UserAgent;
use Digest::MD5 qw(md5_hex);

my $host = $ARGV[0];

if ($host !~ /(http:|https)/) {
    $host = "http://$ARGV[0]";
}

sub showHelp($)
{
    my ($str) = @_;
    print $str,"\n";
    die "$0 ESHOST:ESPORT (ip|host|md5|email|uri) filename tag1 [tag2..tagN]";
}

showHelp("Missing arguments") if (@ARGV < 4);
showHelp("Must be ip, host, or md5 for file type instead of $ARGV[1]") if ($ARGV[1] !~ /^(host|ip|md5|email|uri)$/);
showHelp("file '$ARGV[2]' not found") if (! -f $ARGV[2]);
showHelp("file '$ARGV[2]' empty") if (-z $ARGV[2]);

my $userAgent = LWP::UserAgent->new(timeout => 20);
my $response = $userAgent->request(HTTP::Request::Common::PUT("$host/tagger",
                               "Content-Type" => "application/json;charset=UTF-8",
                                Content => '{"mappings": {"file": { _all: {enabled: 0}, "properties":{"tags":{"type":"string","index": "no"}, "type": {"type":"string","index": "no"}, "data": {"type":"string","index": "no"}, "md5": {"type":"string","index": "no"}}}}}'));

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
$response = $userAgent->post("$host/tagger/file/$ARGV[2]", "Content-Type" => "application/json;charset=UTF-8", Content => $content);
print $response->content, "\n";

