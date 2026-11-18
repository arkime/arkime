# Simple writer: a normal run finishes every pcap file before exit, and a failed
# pcap write doesn't leave empty pcap files behind, each packet thread can have one
use Test::More tests => 8;
use File::Path qw(make_path remove_tree);
use ArkimeTest;
use JSON;
use strict;

my $value = int(rand() * 1000000);
my $tag   = "writersimple-$value";
my $dir   = "/tmp/arkime-writer-$value";
my $dir2  = "/tmp/arkime-writer2-$value";
make_path($dir, $dir2);

sub runCapture {
    my ($noWrites, $tag, $pcapDir) = @_;
    my $pid = open(my $fh, "-|");
    if (!$pid) {
        open(STDERR, ">&STDOUT");
        my @cmd = ("../capture/capture", "-o", "disablePython=true", "-c", "config.test.ini", "-n", "test", "--tag", $tag,
                   "-o", "packetThreads=2", "-o", "pcapDir=$pcapDir", "--copy", "-r", "pcap/bt-tcp.pcap");
        if ($noWrites) {
            # No regular file is allowed to grow, so the first pcap write fails with EFBIG like a full disk
            $SIG{XFSZ} = 'IGNORE';
            exec("/bin/sh", "-c", 'ulimit -f 0; exec "$@"', "sh", @cmd) or exit 1;
        }
        exec(@cmd) or exit 1;
    }
    my $out = do { local $/; <$fh> };
    close($fh);
    return ($out, $? >> 8);
}

sub dirFiles {
    my ($d) = @_;
    opendir(my $dh, $d) or die "opendir $d failed $!";
    my @files = grep { !/^\.\.?$/ } readdir($dh);
    closedir($dh);
    return @files;
}

# Normal run, every files doc must be finished and match what is on disk.
# Only a smoke test, the exit race needs a slow final write so it can't be hit reliably here.
# With 2 threads the 2 bt-tcp sessions always land on different packet threads, hashSalt is xored in after mixing.
my ($out, $rc) = runCapture(0, "$tag-a", $dir);
is($rc, 0, "capture exited cleanly");
esGet("/_refresh");
my $files = esPost("/tests_files/_search?rest_total_hits_as_int=true", to_json({query => {prefix => {name => $dir}}}));
my @docs = map { $_->{_source} } @{$files->{hits}->{hits}};
cmp_ok(scalar(@docs), '>=', 2, "a pcap file for each packet thread");
is_deeply([grep { !($_->{filesize} > 0 && $_->{packets} > 0) } @docs], [], "files docs updated before exit");
is_deeply([grep { (-s $_->{name} // -1) != $_->{filesize} } @docs], [], "on disk sizes match files docs");
is_deeply([grep { -z "$dir/$_" } dirFiles($dir)], [], "no empty pcap files");

# Failed write, every pcap file is still empty so all are removed
($out, $rc) = runCapture(1, "$tag-b", $dir2);
isnt($rc, 0, "capture failed when it couldn't write");
like($out, qr/ERROR - writing '\Q$dir2\E\//, "reported the write failure");
is_deeply([dirFiles($dir2)], [], "no pcap files left behind");

# Cleanup
esPost("/tests_sessions3-*/_delete_by_query?conflicts=proceed&refresh", to_json({query => {prefix => {tags => $tag}}}));
esPost("/tests_files/_delete_by_query?conflicts=proceed&refresh", to_json({query => {prefix => {name => $dir}}}));
esPost("/tests_files/_delete_by_query?conflicts=proceed&refresh", to_json({query => {prefix => {name => $dir2}}}));
remove_tree($dir, $dir2);
