# Test rulesFiles directories: every *.rules.yml / *.rules.yaml in a directory is
# loaded with default.* first, a rule in a later file with the same name replaces an
# earlier one and disabled: true removes it. Runs capture in --tests mode, no Elasticsearch.
# The live section keeps capture running on the command socket to check dir reloads.
use Test::More tests => 33;
use File::Path qw(make_path remove_tree);
use IO::Socket::UNIX;
use POSIX ":sys_wait_h";
use Cwd;
use strict;

my $dir = "/tmp/arkime-rulesdir-$$";
END { remove_tree($dir) if $dir && !$ENV{KEEP}; }
make_path("$dir/sub");

sub writeFile {
    my ($name, $body) = @_;
    open(my $fh, '>', "$dir/$name") or die "can't write $dir/$name";
    print $fh $body;
    close($fh);
}

sub rule {
    my ($name, $tag, $expression) = @_;
    $expression //= "protocols == tls";
    return "  - name: \"$name\"\n    when: \"afterClassify\"\n    expression: \"$expression\"\n    ops:\n      tags: \"$tag\"\n";
}

sub capture {
    my ($rulesFiles, $extra) = @_;
    $extra //= "";
    my $out = `../capture/capture --tests -c config.test.ini -n test -o 'rulesFiles=$rulesFiles' $extra -r pcap/badcurveball.pcap 2>&1`;
    my %tags;
    $tags{$_} = 1 for map { s/"//gr } map { split /,/ } ($out =~ /"tags":\[([^\]]*)\]/g);
    return (\%tags, $out);
}

# default.* loads first even though it sorts after aaa., so aaa. overrides it
writeFile("default.rules.yml", "version: 1\nrules:\n" . rule("a", "default-a") . rule("b", "default-b") . rule("c", "default-c"));
writeFile("aaa.rules.yaml", "version: 1\nrules:\n" . rule("b", "user-b") . "  - name: \"c\"\n    disabled: true\n");
writeFile("notes.txt", "version: 1\nrules:\n" . rule("txt", "txt"));
writeFile(".hidden.rules.yml", "version: 1\nrules:\n" . rule("hidden", "hidden"));
writeFile("sub/nested.rules.yml", "version: 1\nrules:\n" . rule("nested", "nested"));

my ($tags, $out) = capture($dir, "-o debug=1");
ok($tags->{"default-a"}, "default rule loaded");
ok($tags->{"user-b"}, "user file replaces default rule of the same name");
ok(!$tags->{"default-b"}, "replaced default rule is gone");
ok(!$tags->{"default-c"}, "disabled: true removes the default rule");
ok(!$tags->{"txt"}, "files without a rules suffix are skipped");
ok(!$tags->{"hidden"}, "hidden files are skipped");
ok(!$tags->{"nested"}, "sub directories are not scanned");
like($out, qr/aaa\.rules\.yaml: rule 'b' replaces the one from \S+default\.rules\.yml/, "replace logged");
like($out, qr/aaa\.rules\.yaml: rule 'c' disables the one from \S+default\.rules\.yml/, "disable logged");

# Files and directories mix, a plain file keeps whatever name it has
($tags, $out) = capture("$dir;$dir/notes.txt");
ok($tags->{"txt"} && $tags->{"user-b"}, "a file entry loads alongside a directory entry");

# A later same name rule in a file entry replaces the directory one too
writeFile("later.yaml", "version: 1\nrules:\n" . rule("a", "later-a"));
($tags, $out) = capture("$dir;$dir/later.yaml");
ok($tags->{"later-a"} && !$tags->{"default-a"}, "later file entry replaces a rule from an earlier directory entry");

# default.* in a later entry still loads first
($tags, $out) = capture("$dir/aaa.rules.yaml;$dir/default.rules.yml");
ok($tags->{"user-b"} && !$tags->{"default-b"}, "default.* loads first whatever the entry order");

# Empty and nothing
writeFile("empty.rules.yml", "version: 1\nrules: []\n");
($tags, $out) = capture("$dir/empty.rules.yml");
unlike($out, qr/FATAL/, "rules: [] is a valid file");
($tags, $out) = capture("");
unlike($out, qr/FATAL/, "rulesFiles= turns rules off");
make_path("$dir/none");
($tags, $out) = capture("$dir/none");
unlike($out, qr/FATAL/, "a directory with no rules files is fine");

# Missing explicit entry is still fatal
($tags, $out) = capture("$dir/nope.rules.yml");
like($out, qr/FATAL CONFIG ERROR - Couldn't stat rules files file \S+nope\.rules\.yml/, "missing file is fatal");

# The shipped default rules file parses
($tags, $out) = capture("../release/default.rules.yml");
unlike($out, qr/FATAL/, "release/default.rules.yml loads");

# Rules in the same file never replace each other, names didn't have to be unique
make_path("$dir/dup");
open(my $dfh, '>', "$dir/dup/a.rules.yml") or die;
print $dfh "version: 1\nrules:\n" . rule("same", "dup-first") . rule("same", "dup-second");
close($dfh);
($tags, $out) = capture("$dir/dup");
ok($tags->{"dup-first"} && $tags->{"dup-second"}, "same name rules in one file are both kept");
unlike($out, qr/rule 'same' replaces/, "same file duplicate isn't logged as a replace");
open($dfh, '>', "$dir/dup/b.rules.yml") or die;
print $dfh "version: 1\nrules:\n" . rule("same", "dup-later");
close($dfh);
($tags, $out) = capture("$dir/dup");
ok($tags->{"dup-later"} && !$tags->{"dup-first"} && !$tags->{"dup-second"}, "a later file replaces every earlier rule with the name");

# A scalar in a sequence is a clean config error, not a crash
make_path("$dir/seq");
open(my $sfh, '>', "$dir/seq/x.rules.yml") or die;
print $sfh "version: 1\nrules:\n  - - name\n    - disabled\n";
close($sfh);
($tags, $out) = capture("$dir/seq");
like($out, qr/FATAL CONFIG ERROR - \S+x\.rules\.yml: name required for rule/, "rule that is a list is a config error");
is($? & 127, 0, "rule that is a list doesn't crash capture");

# TODO, remove in 7.1 with the skip in rules.c
# The old viewer override name in a rules dir is skipped, not fatal
make_path("$dir/fp");
open(my $ffh, '>', "$dir/fp/featherprint.rules.yaml") or die;
print $ffh "macSourceRank:\n  dhcp: 1\nclassify: []\n";
close($ffh);
open($ffh, '>', "$dir/fp/local.rules.yml") or die;
print $ffh "version: 1\nrules:\n" . rule("fp", "fp-local");
close($ffh);
($tags, $out) = capture("$dir/fp");
unlike($out, qr/FATAL/, "featherprint.rules.yaml in a rules dir isn't fatal");
like($out, qr/Skipping \S+featherprint\.rules\.yaml, it isn't used anymore and should be deleted/, "featherprint.rules.yaml skip is logged");
ok($tags->{"fp-local"}, "rules next to featherprint.rules.yaml still load");

################################################################################
# Live reloads. Capture waits on the command socket so the 10 second reload check runs.
# - An empty file in the dir doesn't stop reloads
# - A new file is only loaded once it stops changing, a half written one never is
# - A same size rewrite during or right after a reload is still picked up
################################################################################
sub slurp {
    my ($file) = @_;
    local $/;
    open(my $fh, '<', $file) or return "";
    my $data = <$fh>;
    close($fh);
    return $data;
}

sub waitFor {
    my ($file, $re, $count, $timeout) = @_;
    my $deadline = time() + $timeout;
    while (time() < $deadline) {
        my @m = (slurp($file) =~ /$re/g);
        return 1 if (scalar @m >= $count);
        select(undef, undef, undef, 0.1);
    }
    return 0;
}

my $live = "$dir/live";
my $sockPath = "$dir/cmd.sock";
my $log = "$dir/live.log";
my $pcap = getcwd() . "/pcap/badcurveball.pcap";
make_path($live);
open(my $lfh, '>', "$live/default.rules.yml") or die;
print $lfh "version: 1\nrules:\n" . rule("a", "live-default-a");
close($lfh);
open($lfh, '>', "$live/empty.rules.yml") or die;
close($lfh);

my $pid = fork();
die "fork failed: $!" unless defined $pid;
if ($pid == 0) {
    open(STDOUT, '>', $log) or die "open $log: $!";
    open(STDERR, '>&', STDOUT);
    exec("../capture/capture", "--tests", "-c", "config.test.ini", "-n", "test", "-o", "rulesFiles=$live",
         "--command-socket", $sockPath, "--command-wait", "--debug");
    die "exec failed: $!";
}

my $ready = 0;
for (my $i = 0; $i < 100 && !$ready; $i++) {
    $ready = -S $sockPath;
    select(undef, undef, undef, 0.1) unless $ready;
}
# The socket is up before the rules are, wait for the startup load to see the empty file
$ready = $ready && waitFor($log, qr/empty\.rules\.yml has no rules/, 1, 30);
ok($ready, "live: capture started");

SKIP: {
    skip "capture did not start", 7 unless $ready;

    # Half a file that would be a parse error, finished well before two checks can see it
    open($lfh, '>', "$live/new.rules.yml") or die;
    print $lfh "version: 1\nrules:\n  - name: \"n\"\n    when: \"afterClassify\"\n    expression: \"protocols == tls";
    close($lfh);
    sleep(3);
    open($lfh, '>', "$live/new.rules.yml") or die;
    print $lfh "version: 1\nrules:\n" . rule("n", "live-n1");
    close($lfh);
    ok(waitFor($log, qr/Reloading rules files/, 1, 45), "live: new file loaded once it settled with an empty file present");

    # Same size rewrite as soon as the reload starts
    open($lfh, '>', "$live/new.rules.yml") or die;
    print $lfh "version: 1\nrules:\n" . rule("n", "live-n2");
    close($lfh);
    ok(waitFor($log, qr/Reloading rules files/, 2, 45), "live: rewrite during the reload is reloaded too");

    my $sock = IO::Socket::UNIX->new(Peer => $sockPath, Type => SOCK_STREAM);
    ok(defined $sock, "live: connected to command socket");
    skip "could not connect to socket", 4 unless defined $sock;
    $sock->autoflush(1);
    print $sock "add-file --notify $pcap\n";
    while (my $line = <$sock>) {
        last if ($line =~ /^file-(done|error)/);
    }
    print $sock "shutdown\n";
    1 while (<$sock>);
    close($sock);

    my $exited = 0;
    for (my $i = 0; $i < 300 && !$exited; $i++) {
        $exited = waitpid($pid, WNOHANG) == $pid;
        select(undef, undef, undef, 0.1) unless $exited;
    }
    ok($exited, "live: capture exits after shutdown");

    my $out = slurp($log);
    my %tags;
    $tags{$_} = 1 for map { s/"//gr } map { split /,/ } ($out =~ /"tags":\[([^\]]*)\]/g);
    unlike($out, qr/FATAL|Parse error/, "live: half written file was never loaded");
    ok($tags{"live-default-a"}, "live: default rule still loaded");
    ok($tags{"live-n2"} && !$tags{"live-n1"}, "live: latest rewrite is what's loaded");
}
kill('KILL', $pid) if (waitpid($pid, WNOHANG) == 0);
waitpid($pid, 0);
