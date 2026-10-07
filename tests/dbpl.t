# Test db.pl commands, only run against elasticsearch
use Test::More tests => 18;
use Test::Differences;
use File::Temp qw(tempdir);
use ArkimeTest;
use JSON;
use strict;

my $db = "../db/db.pl $ENV{INSECURE} --prefix tests";

# dbpl(cmd, opts)
sub dbpl {
    my $out = `$db $_[1] $ArkimeTest::elasticsearch $_[0] 2>&1`;
    return ($? >> 8, $out);
}

sub getUser {
    esGet("/tests_users/_refresh");
    return esGet("/tests_users/_doc/$_[0]")->{_source};
}

sub cleanup {
    esDelete("/tests_users/_doc/$_?refresh=true") foreach ("dbpl-1", "dbpl-2", "dbpl-3");
}

cleanup();

esPut("/tests_users/_doc/dbpl-1?refresh=true", '{"userId":"dbpl-1","userName":"dbpl-1","enabled":true,"roles":["arkimeUser","usersAdmin"],"createEnabled":true}');
esPut("/tests_users/_doc/dbpl-2?refresh=true", '{"userId":"dbpl-2","userName":"dbpl-2","enabled":true,"roles":["arkimeUser"],"createEnabled":true}');
esPut("/tests_users/_doc/dbpl-3?refresh=true", '{"userId":"dbpl-3","userName":"dbpl-3","enabled":true,"roles":["arkimeUser"],"createEnabled":false}');

# users-update --removeRole usersAdmin clears createEnabled too
my ($rc, $out) = dbpl("users-update 'dbpl-*' --removeRole usersAdmin");
is($rc, 0, "users-update removeRole exit code");
like($out, qr/matched 'dbpl-\*', 2 changed/, "users-update removeRole changed 2 users");

my $user = getUser("dbpl-1");
eq_or_diff($user->{roles}, ["arkimeUser"], "dbpl-1 usersAdmin removed");
ok(!$user->{createEnabled}, "dbpl-1 createEnabled cleared");

$user = getUser("dbpl-2");
eq_or_diff($user->{roles}, ["arkimeUser"], "dbpl-2 legacy createEnabled user has no usersAdmin");
ok(!$user->{createEnabled}, "dbpl-2 createEnabled cleared");

$user = getUser("dbpl-3");
ok(!$user->{createEnabled}, "dbpl-3 untouched");

# users-update --addRole usersAdmin sets createEnabled
($rc, $out) = dbpl("users-update dbpl-3 --addRole usersAdmin");
is($rc, 0, "users-update addRole exit code");
$user = getUser("dbpl-3");
eq_or_diff($user->{roles}, ["arkimeUser", "usersAdmin"], "dbpl-3 usersAdmin added");
ok($user->{createEnabled}, "dbpl-3 createEnabled set");

# expire/rotate reject a bad <num> or --history
foreach my $num ("abc", "-1", "0") {
    ($rc, $out) = dbpl("expire daily $num", "-n");
    like($out, qr/Invalid expire <num>/, "expire rejects num $num");
}
($rc, $out) = dbpl("rotate daily 7 --history x", "-n");
isnt($rc, 0, "rotate --history x exit code");
like($out, qr/--history must be a positive number/, "rotate rejects --history x");

# exported files are only readable by the owner
my $dir = tempdir(CLEANUP => 1);
umask(022);
($rc, $out) = dbpl("users-export $dir/users.json");
is($rc, 0, "users-export exit code");
ok(-f "$dir/users.json", "users-export wrote file");
is((stat("$dir/users.json"))[2] & 0777, 0600, "users-export file mode is 0600");

cleanup();
