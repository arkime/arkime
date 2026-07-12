#!/usr/bin/env perl
# Arkime ClickHouse sessions db management script
#
# SPDX-License-Identifier: Apache-2.0
#
# Schema Versions
#  0 - Before this script existed
#  1 - First version of script; sessions3 table and schema_version table
#  2 - Typed JSON paths + max_dynamic_paths, bloom_filter data-skipping indexes

use strict;
use warnings;
use LWP::UserAgent;
use JSON;
use Getopt::Long;
use Data::Dumper;

our $VERSION = 2;

my $verbose = 0;
my $PARTITION = "monthly";
my $PREFIX = "arkime_";
my $SECURE = 1;
my $CABUNDLE = "";
my $USER = "";
my $PASSWORD = "";
my $DATABASE = "arkime";
my $DATABASE_SET = 0;
my $NO_PROMPT = 0;
my $TIMEOUT = 60;

my $endpoint;
my $sessionsTable;
my $lookupsTable;
my $schemaTable;

################################################################################
sub logmsg
{
    local $| = 1;
    print (scalar localtime() . " ") if ($verbose > 0);
    print ("@_");
}

################################################################################
sub showHelp
{
    my ($str) = @_;

    print "\n", $str, "\n\n" if (defined $str && $str ne "");
    print "$0 [options] http[s]://[user:pass\@]host:port[/database] <verb> [verb-args...]\n";
    print "\n";
    print "Options:\n";
    print "  -v, --verbose              - Verbose, multiple increases level\n";
    print "  --insecure                 - Disable certificate verification for https calls\n";
    print "  --cabundle FILE            - CA bundle to use for https certificate verification\n";
    print "  --partition STR            - Sessions partition granularity: monthly (default), daily, none.\n";
    print "                               Every query pays a per-part cost, so fewer partitions = faster\n";
    print "                               searches; expire still works at day granularity for all values.\n";
    print "  --prefix STR               - Prefix for ClickHouse table names, default arkime_\n";
    print "  --user USER                - ClickHouse user for basic authentication\n";
    print "  --password PASS            - ClickHouse password for basic authentication\n";
    print "  --database DB              - ClickHouse database name, default arkime\n";
    print "  --no-prompt                - Do not prompt for INIT/WIPE confirmation\n";
    print "  --timeout SECONDS          - HTTP timeout, default 60\n";
    print "\n";
    print "Verbs:\n";
    print "  info                       - Print server, database, table, partition, and schema information\n";
    print "  init                       - Drop and recreate the database and sessions table\n";
    print "  upgrade                    - Upgrade the sessions table schema to the current version\n";
    print "  wipe                       - Truncate sessions but keep the schema\n";
    print "  expire <days>              - Drop sessions partitions older than <days> days\n";
    print "  disk-usage                 - Print ClickHouse disk usage per sessions partition\n";
    print "  help                       - Show this help\n";
    print "\n";
    exit 1;
}

################################################################################
sub waitFor
{
    my ($str, $help) = @_;

    print "Type \"$str\" to continue - $help?\n";
    while (1) {
        my $answer = <STDIN>;
        if (!defined $answer) {
            die "ERROR: No input available for '$help', stdin is closed or not a terminal.\n";
        }
        chomp $answer;
        last if ($answer eq $str);
        print "You didn't type \"$str\", for some reason you typed \"$answer\"\n";
    }
}

################################################################################
sub urlEncode
{
    my ($str) = @_;

    $str =~ s/([^A-Za-z0-9\-_.~])/sprintf("%%%02X", ord($1))/eg;
    return $str;
}

################################################################################
sub urlDecode
{
    my ($str) = @_;

    $str =~ tr/+/ /;
    $str =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/eg;
    return $str;
}

################################################################################
sub base64Encode
{
    my ($data) = @_;
    my $chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    my $out = "";

    while (length($data) > 0) {
        my $chunk = substr($data, 0, 3, "");
        my $len = length($chunk);
        my @bytes = unpack("C*", $chunk . "\0\0");
        my $n = ($bytes[0] << 16) | ($bytes[1] << 8) | $bytes[2];
        $out .= substr($chars, ($n >> 18) & 63, 1);
        $out .= substr($chars, ($n >> 12) & 63, 1);
        $out .= $len > 1 ? substr($chars, ($n >> 6) & 63, 1) : "=";
        $out .= $len > 2 ? substr($chars, $n & 63, 1) : "=";
    }

    return $out;
}

################################################################################
sub sqlString
{
    my ($str) = @_;

    $str =~ s/\\/\\\\/g;
    $str =~ s/'/\\'/g;
    return "'$str'";
}

################################################################################
sub sqlIdentifier
{
    my ($str) = @_;

    die "Invalid ClickHouse identifier '$str'\n" if ($str !~ /^[A-Za-z_][A-Za-z0-9_]*$/);
    return "`$str`";
}

################################################################################
sub sqlTable
{
    my ($database, $table) = @_;

    return sqlIdentifier($database) . "." . sqlIdentifier($table);
}

################################################################################
sub validateOptions
{
    die "--partition must be monthly, daily, or none\n" if ($PARTITION !~ /^(monthly|daily|none)$/);
    die "--prefix can only contain letters, digits, and underscores\n" if ($PREFIX !~ /^[A-Za-z0-9_]*$/);
    $PREFIX .= "_" if ($PREFIX ne "" && $PREFIX !~ /_$/);
    die "--database must start with a letter or underscore and contain only letters, digits, and underscores\n" if ($DATABASE !~ /^[A-Za-z_][A-Za-z0-9_]*$/);
}

################################################################################
sub parseEndpoint
{
    my ($url) = @_;

    if ($url !~ m{^(https?)://([^/]+)(/[^?#]*)?}) {
        showHelp("You must specify the ClickHouse URL as the first argument: http://localhost:8123 or https://localhost:8123");
    }

    my $scheme = $1;
    my $authority = $2;
    my $path = $3 || "";

    if ($authority =~ s/^([^\@]*)\@//) {
        my $userinfo = $1;
        my ($urlUser, $urlPassword) = split(/:/, $userinfo, 2);
        $USER = urlDecode($urlUser) if ($USER eq "");
        $PASSWORD = urlDecode($urlPassword) if (defined $urlPassword && $PASSWORD eq "");
    }

    if (!$DATABASE_SET && $path ne "" && $path ne "/") {
        $path =~ s{^/}{};
        $path =~ s{/.*$}{};
        $DATABASE = urlDecode($path) if ($path ne "");
    }

    $endpoint = "$scheme://$authority";
}

################################################################################
sub chquery
{
    my ($verb, $sql, $body) = @_;

    my $url = $endpoint . "/";
    my $content;
    if (defined $body) {
        $url .= "?query=" . urlEncode($sql);
        $content = $body;
    } else {
        $content = $sql;
    }

    logmsg "POST $url\n" if ($verbose > 2);
    logmsg "POST DATA: ", Dumper($content), "\n" if ($verbose > 3);

    my $response = $main::userAgent->post($url, Content => $content, Content_Type => "text/plain; charset=UTF-8");
    if (int($response->code / 100) != 2) {
        die "Couldn't $verb ClickHouse query; HTTP status " . $response->code . " " . $response->message . "\nSQL: $sql\nError: " . $response->content . "\n";
    }

    logmsg "$verb RESULT: ", Dumper($response->content), "\n" if ($verbose > 3);
    return $response->content;
}

################################################################################
sub createDatabase
{
    logmsg "Creating database $DATABASE\n";
    chquery("CREATE DATABASE", "CREATE DATABASE IF NOT EXISTS " . sqlIdentifier($DATABASE));
}

################################################################################
sub createSchemaVersionTable
{
    logmsg "Creating schema version table $schemaTable\n";
    chquery("CREATE TABLE", <<EOSQL);
CREATE TABLE IF NOT EXISTS $schemaTable (
    version UInt32,
    updated DateTime DEFAULT now()
)
ENGINE = ReplacingMergeTree(updated)
ORDER BY tuple()
EOSQL
}

################################################################################
sub sessionsDDL
{
    my %partitionMap = (
        monthly => "toYYYYMM(lastPacket)",
        daily   => "toYYYYMMDD(lastPacket)",
        none    => "tuple()",
    );
    my $partitionExpr = $partitionMap{$PARTITION};

    return <<EOSQL;
CREATE TABLE IF NOT EXISTS $sessionsTable (
    -- Typed paths are stored as real typed subcolumns: direct reads with no
    -- Dynamic-type dispatch, and they never count against max_dynamic_paths.
    -- Scalars are Nullable so a missing field behaves like ES (no match).
    -- max_dynamic_paths is raised from the default 1024 so the many remaining
    -- Arkime fields don't overflow into the (much slower) shared-data blob.
    fields JSON(
        max_dynamic_paths = 4096,
        \`_id\`                 String,
        node                   Nullable(String),
        \`\@timestamp\`          Nullable(Int64),
        firstPacket            Nullable(Int64),
        lastPacket             Nullable(Int64),
        length                 Nullable(Int64),
        ipProtocol             Nullable(Int64),
        segmentCnt             Nullable(Int64),
        rootId                 Nullable(String),
        \`source.ip\`           Nullable(String),
        \`source.port\`         Nullable(Int64),
        \`source.bytes\`        Nullable(Int64),
        \`source.packets\`      Nullable(Int64),
        srcDataBytes           Nullable(Int64),
        \`destination.ip\`      Nullable(String),
        \`destination.port\`    Nullable(Int64),
        \`destination.bytes\`   Nullable(Int64),
        \`destination.packets\` Nullable(Int64),
        dstDataBytes           Nullable(Int64),
        \`network.bytes\`       Nullable(Int64),
        \`network.packets\`     Nullable(Int64),
        totDataBytes           Nullable(Int64),
        \`client.bytes\`        Nullable(Int64),
        \`server.bytes\`        Nullable(Int64),
        protocol               Array(String),
        protocolCnt            Nullable(Int64),
        tags                   Array(String),
        tagsCnt                Nullable(Int64),
        huntId                 Array(String),
        huntName               Array(String),
        fileId                 Array(Int64),
        packetPos              Array(Int64),
        packetLen              Array(Int64),
        \`http.host\`           Array(String),
        \`http.uri\`            Array(String),
        \`http.useragent\`      Array(String),
        \`http.method\`         Array(String),
        \`http.statuscode\`     Array(Int64)
    ),
    \`_id\`           String                  DEFAULT fields.\`_id\`,
    node             LowCardinality(String)  MATERIALIZED ifNull(fields.node, ''),
    timestamp        DateTime64(3, 'UTC')    MATERIALIZED fromUnixTimestamp64Milli(ifNull(fields.\`\@timestamp\`, 0)),
    firstPacket      DateTime64(3, 'UTC')    MATERIALIZED fromUnixTimestamp64Milli(ifNull(fields.firstPacket, 0)),
    lastPacket       DateTime64(3, 'UTC')    DEFAULT fromUnixTimestamp64Milli(ifNull(fields.lastPacket, 0)),
    \`source.ip\`      String                 MATERIALIZED ifNull(fields.\`source.ip\`, ''),
    \`destination.ip\` String                 MATERIALIZED ifNull(fields.\`destination.ip\`, ''),
    INDEX idx_source_ip      \`source.ip\`      TYPE bloom_filter(0.01) GRANULARITY 4,
    INDEX idx_destination_ip \`destination.ip\` TYPE bloom_filter(0.01) GRANULARITY 4
)
ENGINE = MergeTree
PARTITION BY $partitionExpr
ORDER BY (lastPacket)
SETTINGS index_granularity = 8192,
         -- Required for lightweight (patch part) UPDATEs, so tag/scrub
         -- updates don't rewrite whole parts.
         enable_block_number_column = 1,
         enable_block_offset_column = 1,
         -- Collapse quiet partitions to a single part: every query pays a
         -- per-part cost, and merging also folds in any pending patch parts.
         min_age_to_force_merge_seconds = 300,
         min_age_to_force_merge_on_partition_only = 1;
EOSQL
}

################################################################################
# Data-skipping indexes over JSON typed-path subcolumns. Indexes on JSON
# subcolumns need a recent ClickHouse (25.3+), so they are applied best effort
# and skipped with a warning on servers that don't support them.
sub optionalIndexes
{
    return (
        ["idx_tags",      "fields.tags",                  "bloom_filter(0.01)"],
        ["idx_http_host", "fields.\`http.host\`",         "bloom_filter(0.01)"],
        ["idx_src_port",  "fields.\`source.port\`",       "bloom_filter(0.01)"],
        ["idx_dst_port",  "fields.\`destination.port\`",  "bloom_filter(0.01)"],
    );
}

################################################################################
sub applyOptionalIndexes
{
    foreach my $idx (optionalIndexes()) {
        my ($name, $expr, $type) = @$idx;
        my $ok = eval {
            chquery("ALTER TABLE", "ALTER TABLE $sessionsTable ADD INDEX IF NOT EXISTS $name $expr TYPE $type GRANULARITY 4");
            chquery("ALTER TABLE", "ALTER TABLE $sessionsTable MATERIALIZE INDEX $name");
            1;
        };
        if (!$ok) {
            logmsg "Skipping optional index $name (this ClickHouse version doesn't support indexes on JSON subcolumns)\n";
            logmsg "  $@" if ($verbose > 0);
        }
    }
}

################################################################################
sub createSessionsTable
{
    logmsg "Creating sessions table $sessionsTable\n";
    chquery("CREATE TABLE", sessionsDDL());
}

################################################################################
# Shortcut values synced from the users ES lookups index; the viewer keeps
# this current so expressions like ip.dst=\$my_shortcut can translate to
# `IN (SELECT arrayJoin(ip) FROM lookups WHERE id = ...)`.
sub createLookupsTable
{
    logmsg "Creating lookups table $lookupsTable\n";
    chquery("CREATE TABLE", <<EOSQL);
CREATE TABLE IF NOT EXISTS $lookupsTable (
    id      String,
    updated DateTime DEFAULT now(),
    ip      Array(String),
    string  Array(String),
    number  Array(Float64)
)
ENGINE = ReplacingMergeTree(updated)
ORDER BY (id)
EOSQL
}

################################################################################
sub tableExists
{
    my ($table) = @_;
    my $result = chquery("SELECT", "SELECT count() FROM system.tables WHERE database = " . sqlString($DATABASE) . " AND name = " . sqlString($table) . " FORMAT TSV");

    chomp $result;
    return int($result) > 0;
}

################################################################################
sub getSchemaVersion
{
    my $schemaTableName = $PREFIX . "schema_version";

    return 0 if (!tableExists($schemaTableName));

    my $result = chquery("SELECT", "SELECT max(version) FROM $schemaTable FORMAT TSV");
    chomp $result;
    return 0 if ($result eq "" || $result eq "\\N");
    return int($result);
}

################################################################################
sub setSchemaVersion
{
    my ($version) = @_;

    logmsg "Setting schema version to $version\n";
    chquery("TRUNCATE", "TRUNCATE TABLE $schemaTable");
    chquery("INSERT", "INSERT INTO $schemaTable (version) VALUES ($version)");
}

################################################################################
sub upgradeToCurrent
{
    createDatabase();
    createSchemaVersionTable();
    createSessionsTable();
    createLookupsTable();

    my $current = getSchemaVersion();
    die "ClickHouse schema version $current is newer than this script supports ($VERSION)\n" if ($current > $VERSION);

    if ($current < 1) {
        logmsg "Applying schema version 1 migrations\n";
        # The CREATE TABLE above is the single source of truth. Nothing to add.
        setSchemaVersion(1);
    }

    if ($current < 2) {
        logmsg "Applying schema version 2 migrations\n";

        # IP bloom indexes work on any table since source.ip/destination.ip
        # are real materialized columns.
        chquery("ALTER TABLE", "ALTER TABLE $sessionsTable ADD INDEX IF NOT EXISTS idx_source_ip \`source.ip\` TYPE bloom_filter(0.01) GRANULARITY 4");
        chquery("ALTER TABLE", "ALTER TABLE $sessionsTable ADD INDEX IF NOT EXISTS idx_destination_ip \`destination.ip\` TYPE bloom_filter(0.01) GRANULARITY 4");
        chquery("ALTER TABLE", "ALTER TABLE $sessionsTable MATERIALIZE INDEX idx_source_ip");
        chquery("ALTER TABLE", "ALTER TABLE $sessionsTable MATERIALIZE INDEX idx_destination_ip");

        applyOptionalIndexes();

        # Typed JSON paths can only be set at CREATE TABLE time; warn if this
        # table predates them.
        my $fieldsType = chquery("SELECT", "SELECT type FROM system.columns WHERE database = " . sqlString($DATABASE) . " AND table = " . sqlString($PREFIX . "sessions3") . " AND name = 'fields' FORMAT TSV");
        if ($fieldsType !~ /max_dynamic_paths/) {
            logmsg "NOTE: the sessions table was created before typed JSON paths existed.\n";
            logmsg "NOTE: run '$0 <url> init' to recreate it for full query performance (deletes session metadata, NOT pcap files).\n";
        }

        setSchemaVersion(2);
    }

    logmsg "ClickHouse schema is current at version $VERSION\n";
}

################################################################################
sub cmdInfo
{
    logmsg "ClickHouse server version:\n";
    print chquery("SELECT", "SELECT version() FORMAT TSV");

    logmsg "\nDatabase $DATABASE exists:\n";
    print chquery("SELECT", "SELECT count() FROM system.databases WHERE name = " . sqlString($DATABASE) . " FORMAT TSV");

    logmsg "Sessions table $sessionsTable exists:\n";
    my $sessionsExists = tableExists($PREFIX . "sessions3");
    print ($sessionsExists ? "1\n" : "0\n");

    if (!$sessionsExists) {
        my $others = chquery("SELECT", "SELECT name FROM system.tables WHERE database = " . sqlString($DATABASE) . " AND name LIKE '%sessions3' FORMAT TSV");
        chomp $others;
        if ($others ne "") {
            foreach my $other (split(/\n/, $others)) {
                (my $otherPrefix = $other) =~ s/sessions3$//;
                logmsg "NOTE: found sessions table '$other' - try --prefix $otherPrefix\n";
            }
        }
    }

    logmsg "Schema version table $schemaTable exists:\n";
    my $schemaExists = tableExists($PREFIX . "schema_version");
    print ($schemaExists ? "1\n" : "0\n");

    if ($sessionsExists) {
        logmsg "Sessions fields column type:\n";
        print chquery("SELECT", "SELECT type FROM system.columns WHERE database = " . sqlString($DATABASE) . " AND table = " . sqlString($PREFIX . "sessions3") . " AND name = 'fields' FORMAT TSV");

        logmsg "Sessions skipping indexes:\n";
        print chquery("SELECT", "SELECT name, expr, type_full FROM system.data_skipping_indices WHERE database = " . sqlString($DATABASE) . " AND table = " . sqlString($PREFIX . "sessions3") . " FORMAT TSVWithNames");

        logmsg "Sessions row count:\n";
        print chquery("SELECT", "SELECT count() FROM $sessionsTable FORMAT TSV");

        logmsg "Sessions partitions:\n";
        print chquery("SELECT", "SELECT partition, min(min_time) AS firstPacket, max(max_time) AS lastPacket, sum(rows) AS rows, formatReadableSize(sum(bytes_on_disk)) AS bytes FROM system.parts WHERE active AND database = " . sqlString($DATABASE) . " AND table = " . sqlString($PREFIX . "sessions3") . " GROUP BY partition ORDER BY partition FORMAT TSVWithNames");
    }

    if ($schemaExists) {
        logmsg "Schema version row:\n";
        print chquery("SELECT", "SELECT version, updated FROM $schemaTable ORDER BY updated DESC LIMIT 1 FORMAT TSVWithNames");
    }
}

################################################################################
sub cmdInit
{
    if (!$NO_PROMPT) {
        logmsg "This will delete ALL Arkime ClickHouse data in database $DATABASE! (It does not delete the pcap files on disk.)\n\n";
        waitFor("INIT", "do you want to erase everything");
    }

    logmsg "Dropping database $DATABASE\n";
    chquery("DROP DATABASE", "DROP DATABASE IF EXISTS " . sqlIdentifier($DATABASE));
    createDatabase();
    createSessionsTable();
    createSchemaVersionTable();
    upgradeToCurrent();
}

################################################################################
sub cmdWipe
{
    if (!$NO_PROMPT) {
        logmsg "This will delete ALL session data in ClickHouse table $sessionsTable! (It does not delete the pcap files on disk or schema.)\n\n";
        waitFor("WIPE", "do you want to wipe everything");
    }

    logmsg "Truncating sessions table $sessionsTable\n";
    chquery("TRUNCATE", "TRUNCATE TABLE $sessionsTable");
}

################################################################################
sub cmdExpire
{
    my ($days) = @_;

    showHelp("expire requires a positive integer number of days") if (!defined $days || $days !~ /^\d+$/ || int($days) < 1);

    logmsg "Finding partitions fully older than $days days based on lastPacket\n";
    my $result = chquery("SELECT", "SELECT partition FROM system.parts WHERE active AND database = " . sqlString($DATABASE) . " AND table = " . sqlString($PREFIX . "sessions3") . " AND partition NOT LIKE 'patch%' GROUP BY partition HAVING max(max_time) < now() - INTERVAL " . int($days) . " DAY ORDER BY partition FORMAT TSV");
    my @partitions = grep { $_ ne "" } split(/\n/, $result);

    foreach my $partition (@partitions) {
        logmsg "Dropping partition $partition\n";
        chquery("ALTER TABLE", "ALTER TABLE $sessionsTable DROP PARTITION " . sqlString($partition));
    }

    # Partitions straddling the cutoff (and --partition none tables) still hold
    # expired rows; remove them with a lightweight DELETE.
    logmsg "Deleting remaining sessions older than $days days\n";
    chquery("DELETE", "DELETE FROM $sessionsTable WHERE lastPacket < now() - INTERVAL " . int($days) . " DAY");
}

################################################################################
sub cmdDiskUsage
{
    logmsg "Disk usage for $sessionsTable:\n";
    print chquery("SELECT", "SELECT partition, count() AS parts, sum(rows) AS rows, formatReadableSize(sum(data_compressed_bytes)) AS compressed, formatReadableSize(sum(data_uncompressed_bytes)) AS uncompressed, formatReadableSize(sum(bytes_on_disk)) AS bytes_on_disk FROM system.parts WHERE active AND database = " . sqlString($DATABASE) . " AND table = " . sqlString($PREFIX . "sessions3") . " GROUP BY partition ORDER BY partition FORMAT TSVWithNames");
}

################################################################################
Getopt::Long::Configure("bundling");
GetOptions(
    "v|verbose+" => \$verbose,
    "insecure" => sub { $SECURE = 0; },
    "partition=s" => \$PARTITION,
    "cabundle=s" => \$CABUNDLE,
    "prefix=s" => \$PREFIX,
    "user=s" => \$USER,
    "password=s" => \$PASSWORD,
    "database=s" => sub { $DATABASE = $_[1]; $DATABASE_SET = 1; },
    "no-prompt" => \$NO_PROMPT,
    "timeout=i" => \$TIMEOUT,
    "help" => sub { showHelp("Help:"); }
) or showHelp("Unknown global option");

showHelp("Missing arguments") if (@ARGV < 2);
showHelp("Help:") if ($ARGV[1] eq "help");

parseEndpoint($ARGV[0]);
validateOptions();

$sessionsTable = sqlTable($DATABASE, $PREFIX . "sessions3");
$lookupsTable = sqlTable($DATABASE, $PREFIX . "lookups");
$schemaTable = sqlTable($DATABASE, $PREFIX . "schema_version");

$main::userAgent = LWP::UserAgent->new(timeout => $TIMEOUT, keep_alive => 5);
if ($USER ne "" || $PASSWORD ne "") {
    $main::userAgent->default_header('Authorization' => "Basic " . base64Encode("$USER:$PASSWORD"));
}

$main::userAgent->ssl_opts(
    SSL_verify_mode => $SECURE,
    verify_hostname => $SECURE,
    ($CABUNDLE ne "" ? (SSL_ca_file => $CABUNDLE) : ())
);

my $command = $ARGV[1];
if ($command eq "info") {
    cmdInfo();
} elsif ($command eq "init") {
    cmdInit();
} elsif ($command eq "upgrade") {
    upgradeToCurrent();
} elsif ($command eq "wipe") {
    cmdWipe();
} elsif ($command eq "expire") {
    cmdExpire($ARGV[2]);
} elsif ($command eq "disk-usage") {
    cmdDiskUsage();
} else {
    showHelp("Unknown command '$command'");
}

1;
