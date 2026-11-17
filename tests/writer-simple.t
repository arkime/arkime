# Simple writer opens pcap files lazily in its writer thread, make sure exit
# waits for the final write/files update and bad pcapDirs still fail at startup
use Test::More tests => 10;
use Cwd qw(realpath);
use File::Path qw(make_path remove_tree);
use URI::Escape;
use ArkimeTest;
use JSON;
use strict;

my $value = int(rand() * 1000000);
my $tag   = "writersimple-$value";
my $dir   = "/tmp/arkime-writer-$value";
make_path($dir);
my $realDir = realpath($dir);

sub runCapture {
    my ($tag, @args) = @_;
    my $pid = open(my $fh, "-|");
    if (!$pid) {
        open(STDERR, ">&STDOUT");
        exec("../capture/capture", "-o", "disablePython=true", "-c", "config.test.ini", "-n", "test", "--tag", $tag, @args) or exit 1;
    }
    my $out = do { local $/; <$fh> };
    close($fh);
    return ($out, $? >> 8);
}

sub filesDocs {
    my ($prefix) = @_;
    esGet("/_refresh");
    return esPost("/tests_files/_search?rest_total_hits_as_int=true", to_json({query => {prefix => {name => $prefix}}}));
}

# Normal run, files doc must be finished and match what is on disk
my ($out, $rc) = runCapture("$tag-a", "--copy", "-r", "pcap/bt-tcp.pcap", "-o", "pcapDir=$dir");
is($rc, 0, "capture exited cleanly");

# One file per packet thread that saw packets
my $files = filesDocs($realDir);
ok($files->{hits}->{total} >= 1, "files docs created");
my @docs = map { $_->{_source} } @{$files->{hits}->{hits}};
is_deeply([grep { !($_->{filesize} > 0 && $_->{packets} > 0) } @docs], [], "files docs updated before exit");
is_deeply([grep { (-s $_->{name} // -1) != $_->{filesize} } @docs], [], "on disk sizes match files docs");

opendir(my $dh, $dir);
my @left = grep { !/^\.\.?$/ && ($_ =~ /-probe-/ || -z "$dir/$_") } readdir($dh);
closedir($dh);
is_deeply(\@left, [], "no probe or empty files left behind");

# pcapDir that can't hold files must fail before any files doc is made
my $bad = "$dir/notadir";
open(my $fh, ">", $bad) or die "create $bad failed $!";
close($fh);
($out, $rc) = runCapture("$tag-b", "--copy", "-r", "pcap/bt-tcp.pcap", "-o", "pcapDir=$bad");
isnt($rc, 0, "capture failed with bad pcapDir");
like($out, qr/pcap open failed/, "reported open failure");
$files = filesDocs(realpath($bad));
is($files->{hits}->{total}, 0, "no files doc for bad pcapDir");
countTest(0, "date=-1&expression=" . uri_escape("tags=$tag-b"));

# Cleanup
esPost("/tests_sessions3-*/_delete_by_query?conflicts=proceed&refresh", to_json({query => {prefix => {tags => $tag}}}));
esPost("/tests_files/_delete_by_query?conflicts=proceed&refresh", to_json({query => {prefix => {name => $realDir}}}));
remove_tree($dir);
