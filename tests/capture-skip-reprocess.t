# Offline --skip and --reprocess look up the files record by name, make sure
# names with quotes, spaces and & work and can't match other files
use Test::More tests => 8;
use Cwd qw(getcwd realpath);
use File::Copy;
use File::Path qw(make_path remove_tree);
use URI::Escape;
use ArkimeTest;
use JSON;
use strict;

my $value = int(rand() * 1000000);
my $tag   = "skipreprocess-$value";
my $dir   = "/tmp/arkime-skip-$value";
my $dir2  = "/tmp/arkime-skip2-$value";
make_path($dir, $dir2);

my $file = "$dir/a\"b c&d.pcap";
copy("pcap/bt-tcp.pcap", $file) or die "copy failed $!";
my $realFile = realpath($file);

sub runCapture {
    my ($tag, @args) = @_;
    my $pid = open(my $fh, "-|");
    if (!$pid) {
        open(STDERR, ">&STDOUT");
        exec("../capture/capture", "-o", "disablePython=true", "-c", "config.test.ini", "-n", "test", "--tag", $tag, @args) or exit 1;
    }
    my $out = do { local $/; <$fh> };
    close($fh);
    return $out;
}

# Ingest once and learn how many sessions the pcap makes
runCapture("$tag-a", "-r", $file);
esGet("/_refresh");
my $json = viewerGet("/sessions.json?date=-1&expression=" . uri_escape("tags=$tag-a"));
my $count = $json->{recordsFiltered};
ok($count > 0, "ingested $count sessions");

my $files = esPost("/tests_files/_search?rest_total_hits_as_int=true", to_json({query => {term => {name => $realFile}}}));
is($files->{hits}->{total}, 1, "one files record with the exact name");

# --skip finds the existing record for the same name
runCapture("$tag-b", "-R", $dir, "--skip");
esGet("/_refresh");
countTest(0, "date=-1&expression=" . uri_escape("tags=$tag-b"));

# A name crafted to match any file must not be skipped
copy("pcap/bt-tcp.pcap", "$dir2/x\" OR name:* OR name:\"y.pcap") or die "copy failed $!";
runCapture("$tag-c", "-R", $dir2, "--skip");
esGet("/_refresh");
countTest($count, "date=-1&expression=" . uri_escape("tags=$tag-c"));

# --reprocess finds the record, it doesn't create a new one
my $out = runCapture("$tag-d", "-r", $file, "--reprocess");
unlike($out, qr/Can't reprocess/, "reprocess found the files record");
esGet("/_refresh");
$files = esPost("/tests_files/_search?rest_total_hits_as_int=true", to_json({query => {term => {name => $realFile}}}));
is($files->{hits}->{total}, 1, "reprocess reused the files record");

# Cleanup
esPost("/tests_sessions3-*/_delete_by_query?conflicts=proceed&refresh", to_json({query => {prefix => {tags => $tag}}}));
esPost("/tests_files/_delete_by_query?conflicts=proceed&refresh", to_json({query => {prefix => {name => realpath($dir)}}}));
esPost("/tests_files/_delete_by_query?conflicts=proceed&refresh", to_json({query => {prefix => {name => realpath($dir2)}}}));
remove_tree($dir, $dir2);
