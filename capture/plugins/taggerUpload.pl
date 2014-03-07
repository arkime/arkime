#!/usr/bin/perl -w
# Used to upload new tagger files to the database.  The filename is used as
# a database key, so make sure to reuse the same filename.

use strict;
use LWP::UserAgent;
use Digest::MD5 qw(md5_hex);

sub showHelp($)
{
    my ($str) = @_;
    print $str,"\n";
    die "$0 ESHOST:ESPORT (ip|host|md5) filename tag1 [tag2..tagN]";
}

showHelp("Missing arguments") if (@ARGV < 4);
showHelp("Must be ip, host, or md5 for file type instead of $ARGV[1]") if ($ARGV[1] !~ /^(host|ip|md5)$/);
showHelp("file '$ARGV[2]' not found") if (! -f $ARGV[2]);
showHelp("file '$ARGV[2]' empty") if (-z $ARGV[2]);

my $userAgent = LWP::UserAgent->new(timeout => 20);
my $response = $userAgent->post("http://$ARGV[0]/tagger",
                                Content => '{"mappings": {"file": { "_all.enabled" : false, "properties":{"tags":{"type":"string","index": "no"}, "type": {"type":"string","index": "no"}, "data": {"type":"string","index": "no"}, "md5": {"type":"string","index": "no"}}}}}');

my @ELEMENTS;
open (FILE, $ARGV[2]);
while (<FILE>) {
    next if (/^#/ || /^$/);
    chop;
    push(@ELEMENTS, $_);
}
my $elements = join ',', @ELEMENTS;
close (FILE);

my $md5hex = md5_hex($elements);

my $content  = '{"tags": "' . join(',', @ARGV[3 .. $#ARGV]) . '", "md5":"' . $md5hex .'", "type":"' . $ARGV[1] . '", "data":"' . $elements . '"}'. "\n";
#print $content,"\n";
$response = $userAgent->post("http://$ARGV[0]/tagger/file/$ARGV[2]", Content => $content);
print $response->content, "\n";

