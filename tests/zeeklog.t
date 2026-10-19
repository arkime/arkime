# Test the zeeklog plugin writes Zeek style logs, runs capture in --tests mode
# so no Elasticsearch is needed
use Test::More tests => 30;
use Cwd qw(getcwd);
use File::Path qw(make_path remove_tree);
use strict;

my $dir = "/tmp/arkime-zeeklog-" . int(rand() * 1000000);
make_path($dir);

sub runCapture {
    my ($pcap, @extra) = @_;
    system("../capture/capture --tests -c config.test.ini -n test -r pcap/$pcap -o plugins=zeeklog.so -o zeekLogDir=$dir @extra > /dev/null 2>&1");
}

sub readLog {
    my ($name) = @_;
    open(my $fh, "<", "$dir/$name") or return;
    my %log = (rows => []);
    while (my $line = <$fh>) {
        chomp $line;
        if ($line =~ /^#(\w+)[\t ]?(.*)$/) {
            $log{$1} = $2;
        } elsif ($line ne "") {
            push @{$log{rows}}, [split(/\t/, $line, -1)];
        }
    }
    close($fh);
    return \%log;
}

sub col {
    my ($log, $row, $name) = @_;
    my @fields = split(/\t/, $log->{fields});
    for (my $i = 0; $i < @fields; $i++) {
        return $row->[$i] if ($fields[$i] eq $name);
    }
    return undef;
}

# ---- ascii conn.log and dns.log ----
runCapture("dns-mx.pcap");
my $conn = readLog("conn.log");
ok($conn, "conn.log written");
is($conn->{separator}, '\x09', "conn.log separator header");
is($conn->{path}, "conn", "conn.log path header");
like($conn->{fields}, qr/^ts\tuid\tid\.orig_h\tid\.orig_p\tid\.resp_h\tid\.resp_p\tproto\tservice\tduration/, "conn.log zeek column order");
like($conn->{types}, qr/^time\tstring\taddr\tport\taddr\tport\tenum\tstring\tinterval/, "conn.log zeek column types");
ok(defined $conn->{close}, "conn.log #close written at exit");
is(scalar @{$conn->{rows}}, 1, "conn.log one row");
my $row = $conn->{rows}[0];
is(col($conn, $row, "id.orig_h"), "10.180.156.185", "conn orig_h");
is(col($conn, $row, "id.resp_p"), "53", "conn resp_p");
is(col($conn, $row, "proto"), "udp", "conn proto");
is(col($conn, $row, "service"), "dns", "conn service drops transports");
is(col($conn, $row, "conn_state"), "SF", "conn udp conn_state");
is(col($conn, $row, "orig_ip_bytes"), "52", "conn ip bytes strip ethernet header");
is(col($conn, $row, "local_orig"), "-", "conn unset column is -");
like(col($conn, $row, "uid"), qr/^C[0-9A-Za-z]{17}$/, "conn zeek style uid");

my $dns = readLog("dns.log");
ok($dns, "dns.log written");
is(scalar @{$dns->{rows}}, 1, "dns.log one row per query");
$row = $dns->{rows}[0];
is(col($dns, $row, "uid"), col($conn, $conn->{rows}[0], "uid"), "dns uid matches conn uid");
is(col($dns, $row, "query"), "mx.com", "dns query");
is(col($dns, $row, "qtype_name"), "MX", "dns qtype_name");
is(col($dns, $row, "rcode_name"), "NOERROR", "dns rcode_name");
is(col($dns, $row, "rtt"), "0.082653", "dns rtt from query/response times");
is(col($dns, $row, "answers"), "cluster5.us.messagelabs.com,cluster5a.us.messagelabs.com", "dns answers only answer section");
is(col($dns, $row, "TTLs"), "3600.000000,3600.000000", "dns TTLs as intervals");

# ---- TCP conn_state/history and http.log ----
remove_tree($dir); make_path($dir);
runCapture("http-simple-get.pcap");
$conn = readLog("conn.log");
is(col($conn, $conn->{rows}[0], "conn_state"), "SF", "tcp conn_state from flag counters");
is(col($conn, $conn->{rows}[0], "history"), "ShADadFf", "tcp history from flag counters");
my $http = readLog("http.log");
is(col($http, $http->{rows}[0], "uri"), "/", "http uri is the path");
is(col($http, $http->{rows}[0], "status_code"), "403", "http status_code");

# ---- json output and a custom log section ----
remove_tree($dir); make_path($dir);
open(my $cfg, ">", "$dir/config.ini");
open(my $base, "<", "config.test.ini");
print $cfg $_ while (<$base>);
close($base);
print $cfg "\n[zeeklog-conn]\nexclude=local_orig,local_resp,tunnel_parents\nnode=\@node\n\n[zeeklog-mylog]\nprotocols=dns\niterate=dns\nts=\@ts\nquery=\@dns.query\nproto=\@proto\n";
close($cfg);
system("../capture/capture --tests -c $dir/config.ini -n test -r pcap/dns-mx.pcap -o plugins=zeeklog.so -o zeekLogDir=$dir -o zeekLogFormat=json -o 'zeekLogs=*;mylog' > /dev/null 2>&1");
open(my $jfh, "<", "$dir/conn.log");
my $jline = <$jfh>;
close($jfh);
like($jline, qr/^\{"ts":1386104996\.973318,"uid":"C[0-9A-Za-z]{17}","id\.orig_h":"10\.180\.156\.185",.*"node":"test"\}$/, "json conn row with added node column and excluded columns");
open($jfh, "<", "$dir/mylog.log");
$jline = <$jfh>;
close($jfh);
is($jline, qq({"ts":1386104996.973318,"query":"mx.com","proto":"udp"}\n), "custom log section with dns iteration");

remove_tree($dir);
