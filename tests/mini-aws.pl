#!/usr/bin/env perl
#
# SPDX-License-Identifier: Apache-2.0
#
# Mini AWS server for testing (S3 + a minimal SQS). Stores everything in
# memory, ignores auth. Non-blocking I/O with per-connection buffering to
# prevent one slow client from starving others.
# Supports: CreateMultipartUpload, UploadPart, CompleteMultipartUpload,
#           PutObject, GetObject, DeleteObject, HeadObject,
#           ListBuckets, ListObjects
# Also a minimal SQS emulation compatible with Arkime's reader-scheme-sqs:
#   POST /{account}/{queue}?Action=ReceiveMessage        -- long-poll receive
#   POST /{account}/{queue}  body Action=DeleteMessage&ReceiptHandle=...
#   POST /{account}/{queue}  body Action=SendMessage&MessageBody=...
# Plus S3 bucket notification configuration (as used by
# `aws s3api put-bucket-notification-configuration`) that publishes to an
# SQS queue on object create/delete:
#   PUT /{bucket}?notification   (XML NotificationConfiguration w/ <Queue> ARNs)
#   GET /{bucket}?notification
# Plus a test helper to enqueue an S3-event message directly:
#   POST /_sqs_enqueue/{account}/{queue}?bucket=B&key=K

use strict;
use warnings;
STDOUT->autoflush(1);
STDERR->autoflush(1);
$SIG{INT} = sub { exit 0; };
use IO::Socket::IP;
use IO::Select;
use Fcntl qw(F_GETFL F_SETFL O_NONBLOCK);
use Digest::MD5 qw(md5_hex);
use POSIX qw(strftime);
use File::Path qw(make_path);

my $debug = 0;
if (@ARGV && $ARGV[0] eq '--debug') {
    $debug = 1;
    shift @ARGV;
}

my $port = $ARGV[0] || 9000;
my $USE_DISK = 0;
my $DISK_DIR = "/tmp/mini-aws";

# Storage: $objects{$bucket}{$key} = { data => ..., content_type => ..., etag => ..., size => ..., last_modified => ... }
my %objects;

# Multipart: $uploads{$uploadId} = { bucket => ..., key => ..., parts => { $partNum => { data => ..., etag => ... } } }
my %uploads;
my $nextUploadId = 1;

# SQS queues: $sqs_queues{"$account/$queue"} = [ { ReceiptHandle => ..., Body => ..., invisible => 0|1 }, ... ]
my %sqs_queues;
my $nextReceipt = 1;
my $nextMsgId   = 1;

# Bucket notification configs: $bucket_notifications{$bucket} = [ { queue => "acct/queue", events => [...] }, ... ]
my %bucket_notifications;

# Per-connection state
my %conn_buf;    # fileno => raw read buffer
my %conn_state;  # fileno => { method, uri, query, headers, content_length, body, headers_done }

my $server = IO::Socket::IP->new(
    LocalHost => '::',
    LocalPort => $port,
    Proto     => 'tcp',
    Listen    => 128,
    ReuseAddr => 1,
    V6Only    => 0,
);
if (!$server) {
    # Fall back to IPv4-only (e.g. CI runner without IPv6)
    my $err = $!;
    $server = IO::Socket::IP->new(
        LocalHost => '0.0.0.0',
        LocalPort => $port,
        Proto     => 'tcp',
        Listen    => 128,
        ReuseAddr => 1,
    ) or die "Cannot start server on port $port: dual-stack=$err v4=$!\n";
    print "Mini AWS: dual-stack bind failed ($err), using IPv4-only\n";
}

my $flags = fcntl($server, F_GETFL, 0);
fcntl($server, F_SETFL, $flags | O_NONBLOCK);

print "Mini AWS listening on port $port\n";

my $sel = IO::Select->new($server);

while (1) {
    my @ready = $sel->can_read(1);

    for my $fh (@ready) {
        if ($fh == $server) {
            while (my $client = $server->accept()) {
                $client->autoflush(1);
                binmode($client);
                my $cf = fcntl($client, F_GETFL, 0);
                fcntl($client, F_SETFL, $cf | O_NONBLOCK);
                my $fn = fileno($client);
                $conn_buf{$fn} = '';
                $conn_state{$fn} = {};
                $sel->add($client);
            }
            next;
        }

        my $fn = fileno($fh);
        my $data;
        my $n = sysread($fh, $data, 65536);
        if (!defined $n) {
            next if $!{EAGAIN} || $!{EWOULDBLOCK};
            cleanup_conn($fh, $fn);
            next;
        }
        if ($n == 0) {
            cleanup_conn($fh, $fn);
            next;
        }
        $conn_buf{$fn} .= $data;

        # Try to parse and handle complete HTTP requests from the buffer
        while (length($conn_buf{$fn}) > 0) {
            my $state = $conn_state{$fn} //= {};

            # Parse headers if not done yet
            if (!$state->{headers_done}) {
                my $hdr_end = index($conn_buf{$fn}, "\r\n\r\n");
                last if $hdr_end < 0;  # incomplete headers

                my $header_block = substr($conn_buf{$fn}, 0, $hdr_end);
                substr($conn_buf{$fn}, 0, $hdr_end + 4, '');

                my @lines = split(/\r\n/, $header_block);
                my $request_line = shift @lines;
                if ($request_line =~ m{^(\S+)\s+(\S+)\s+HTTP/}) {
                    $state->{method} = $1;
                    my $full_uri = $2;
                    if ($full_uri =~ /^([^?]*)(?:\?(.*))?$/) {
                        $state->{uri} = $1;
                        $state->{query} = $2 // '';
                    } else {
                        $state->{uri} = $full_uri;
                        $state->{query} = '';
                    }
                } else {
                    cleanup_conn($fh, $fn);
                    last;
                }

                my %headers;
                for my $line (@lines) {
                    if ($line =~ /^([^:]+):\s*(.*)$/) {
                        $headers{lc($1)} = $2;
                    }
                }
                $state->{headers} = \%headers;
                $state->{content_length} = int($headers{'content-length'} // 0);
                $state->{body} = '';
                $state->{headers_done} = 1;

                # Handle Expect: 100-continue — client won't send body until we respond
                if (($headers{'expect'} // '') =~ /100-continue/i) {
                    write_all($fh, "HTTP/1.1 100 Continue\r\n\r\n");
                }
            }

            # Accumulate body
            my $need = $state->{content_length} - length($state->{body});
            if ($need > 0) {
                my $avail = length($conn_buf{$fn});
                if ($avail == 0) { last; }
                my $take = ($avail < $need) ? $avail : $need;
                $state->{body} .= substr($conn_buf{$fn}, 0, $take, '');
                $need -= $take;
            }
            last if $need > 0;  # still waiting for body

            # We have a complete request — handle it
            my $resp = handle_request($state);
            if ($resp eq '_shutdown') {
                write_all($fh, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                cleanup_conn($fh, $fn);
                exit(0);
            }

            if ($debug) {
                my ($status) = $resp =~ m{^HTTP/\S+\s+(\d+\s+[^\r\n]*)};
                $status //= '???';
                my $range = $state->{headers}{'range'} // '';
                my $body_size = length($state->{body} // '');
                my $ts2 = strftime("%H:%M:%S", localtime);
                print "$ts2 AWS: $state->{method} $state->{uri}" . ($state->{query} ne '' ? "?$state->{query}" : "") . " body:$body_size" . ($range ne '' ? " Range:$range" : "") . " -> $status\n";
            }

            write_all($fh, $resp);

            # Check for Connection: close
            my $conn_hdr = $state->{headers}{'connection'} // '';
            $conn_state{$fn} = {};
            if (lc($conn_hdr) eq 'close') {
                cleanup_conn($fh, $fn);
                last;
            }
        }
    }
}

sub cleanup_conn {
    my ($fh, $fn) = @_;
    delete $conn_buf{$fn};
    delete $conn_state{$fn};
    $sel->remove($fh);
    close($fh);
}

# Write all data to a non-blocking socket, waiting for writability as needed
sub write_all {
    my ($fh, $data) = @_;
    my $offset = 0;
    my $len = length($data);
    while ($offset < $len) {
        my $n = syswrite($fh, $data, $len - $offset, $offset);
        if (defined $n) {
            $offset += $n;
            next;
        }
        if ($!{EAGAIN} || $!{EWOULDBLOCK}) {
            IO::Select->new($fh)->can_write(5);
            next;
        }
        return;  # real error, give up
    }
}

# Build a complete HTTP response string
sub build_response {
    my ($status, $status_text, $headers, $body) = @_;
    $body //= '';
    my $resp = "HTTP/1.1 $status $status_text\r\n";
    $headers->{'Content-Length'} //= length($body);
    for my $k (keys %$headers) {
        $resp .= "$k: $headers->{$k}\r\n";
    }
    $resp .= "\r\n";
    $resp .= $body;
    return $resp;
}

sub handle_request {
    my ($state) = @_;
    my $method  = $state->{method};
    my $uri     = $state->{uri};
    my $query   = $state->{query};
    my $headers = $state->{headers};
    my $body    = $state->{body};

    my $range = $headers->{'range'} // '';
    my $body_size = length($body);

    if ($uri eq '/_shutdown') {
        return '_shutdown';
    }

    # SQS test helper: enqueue an S3-event style message
    if ($method eq 'POST' && $uri =~ m{^/_sqs_enqueue/([^/]+)/([^/]+)/?$}) {
        return sqs_enqueue_resp($1, $2, $query, $body);
    }

    # SQS: detect Action in query (ReceiveMessage) or body (Delete/Send)
    if ($method eq 'POST' && $uri =~ m{^/([^/]+)/([^/]+)/?$}) {
        my ($acct, $queue) = ($1, $2);
        my $action;
        if ($query =~ /(?:^|&)Action=([^&]+)/) { $action = $1; }
        elsif ($body =~ /(?:^|&)Action=([^&]+)/) { $action = $1; }
        if (defined $action) {
            return sqs_receive_resp($acct, $queue, $query) if $action eq 'ReceiveMessage';
            return sqs_delete_resp($acct, $queue, $body)    if $action eq 'DeleteMessage';
            return sqs_send_resp($acct, $queue, $body)      if $action eq 'SendMessage';
        }
    }

    my ($bucket, $key) = parse_path($uri);
    print "  bucket=$bucket key=" . ($key // '') . "\n" if ($debug && defined $bucket);

    # S3 bucket notification configuration
    if (defined $bucket && !defined $key && $query =~ /(?:^|&)notification(?:=|&|$)/) {
        if ($method eq 'PUT') {
            return put_bucket_notification_resp($bucket, $body);
        } elsif ($method eq 'GET') {
            return get_bucket_notification_resp($bucket);
        } elsif ($method eq 'DELETE') {
            delete $bucket_notifications{$bucket};
            return build_response(204, 'No Content', {}, '');
        }
    }

    if ($method eq 'POST' && $query =~ /uploads\b/ && !($query =~ /uploadId/)) {
        return create_multipart_upload($bucket, $key, $headers);
    } elsif ($method eq 'PUT' && $query =~ /uploadId=([^&]+)/) {
        my $uploadId = $1;
        $query =~ /partNumber=(\d+)/;
        my $partNum = $1;
        return upload_part($uploadId, $partNum, $body);
    } elsif ($method eq 'POST' && $query =~ /uploadId=([^&]+)/) {
        return complete_multipart_upload($1, $body);
    } elsif ($method eq 'GET' && !defined $bucket) {
        return list_buckets_resp();
    } elsif ($method eq 'GET' && !defined $key && defined $bucket) {
        return list_objects_resp($bucket, $query);
    } elsif ($method eq 'PUT' && defined $key) {
        return put_object_resp($bucket, $key, $body, $headers->{'content-type'});
    } elsif ($method eq 'GET' && defined $key) {
        return get_object_resp($bucket, $key, $headers->{'range'});
    } elsif ($method eq 'DELETE' && defined $key) {
        return delete_object_resp($bucket, $key);
    } elsif ($method eq 'HEAD' && defined $key) {
        return head_object_resp($bucket, $key);
    } else {
        return send_s3_error_resp(501, "NotImplemented", "Not implemented: $method $uri?$query");
    }
}

sub parse_path {
    my ($path) = @_;
    if ($path =~ m{^/([^/]+)/(.+)$}) {
        return ($1, $2);
    } elsif ($path =~ m{^/([^/]+)/?$}) {
        return ($1, undef);
    }
    return (undef, undef);
}

sub amz_date {
    return strftime("%a, %d %b %Y %H:%M:%S GMT", gmtime());
}

sub disk_path {
    my ($bucket, $key) = @_;
    return "$DISK_DIR/$bucket/$key";
}

sub save_object {
    my ($bucket, $key, $data, $content_type) = @_;
    my $etag = '"' . md5_hex($data) . '"';
    if ($USE_DISK) {
        my $path = disk_path($bucket, $key);
        my $dir = $path;
        $dir =~ s{/[^/]+$}{};
        make_path($dir) unless -d $dir;
        open(my $fh, '>:raw', $path) or die "Cannot write $path: $!";
        print $fh $data;
        close($fh);
        $objects{$bucket}{$key} = {
            content_type  => $content_type,
            etag          => $etag,
            size          => length($data),
            last_modified => amz_date(),
        };
    } else {
        $objects{$bucket}{$key} = {
            data          => $data,
            content_type  => $content_type,
            etag          => $etag,
            size          => length($data),
            last_modified => amz_date(),
        };
    }
    fire_bucket_notification($bucket, $key, 'ObjectCreated:Put', length($data), $etag);
    return $etag;
}

sub load_object_data {
    my ($bucket, $key) = @_;
    if ($USE_DISK) {
        my $path = disk_path($bucket, $key);
        open(my $fh, '<:raw', $path) or return undef;
        local $/;
        my $data = <$fh>;
        close($fh);
        return $data;
    } else {
        return $objects{$bucket}{$key}{data};
    }
}

sub create_multipart_upload {
    my ($bucket, $key, $headers) = @_;
    my $uploadId = $nextUploadId++;
    $uploads{$uploadId} = { bucket => $bucket, key => $key, parts => {} };
    my $xml = qq{<?xml version="1.0" encoding="UTF-8"?>
<InitiateMultipartUploadResult>
  <Bucket>$bucket</Bucket>
  <Key>$key</Key>
  <UploadId>$uploadId</UploadId>
</InitiateMultipartUploadResult>};
    return build_response(200, 'OK', { 'Content-Type' => 'application/xml' }, $xml);
}

sub upload_part {
    my ($uploadId, $partNum, $body) = @_;
    my $upload = $uploads{$uploadId};
    unless ($upload) {
        return send_s3_error_resp(404, "NoSuchUpload", "Upload $uploadId not found");
    }
    my $etag = '"' . md5_hex($body) . '"';
    $upload->{parts}{$partNum} = { data => $body, etag => $etag };
    return build_response(200, 'OK', { 'ETag' => $etag }, '');
}

sub complete_multipart_upload {
    my ($uploadId, $body) = @_;
    my $upload = $uploads{$uploadId};
    unless ($upload) {
        return send_s3_error_resp(404, "NoSuchUpload", "Upload $uploadId not found");
    }
    my $data = '';
    for my $partNum (sort { $a <=> $b } keys %{$upload->{parts}}) {
        $data .= $upload->{parts}{$partNum}{data};
    }
    my $etag   = save_object($upload->{bucket}, $upload->{key}, $data, 'application/octet-stream');
    my $bucket = $upload->{bucket};
    my $key    = $upload->{key};
    delete $uploads{$uploadId};
    my $xml = qq{<?xml version="1.0" encoding="UTF-8"?>
<CompleteMultipartUploadResult>
  <Bucket>$bucket</Bucket>
  <Key>$key</Key>
  <ETag>$etag</ETag>
</CompleteMultipartUploadResult>};
    return build_response(200, 'OK', { 'Content-Type' => 'application/xml' }, $xml);
}

sub put_object_resp {
    my ($bucket, $key, $body, $content_type) = @_;
    my $etag = save_object($bucket, $key, $body, $content_type // 'application/octet-stream');
    return build_response(200, 'OK', { 'ETag' => $etag }, '');
}

sub get_object_resp {
    my ($bucket, $key, $range) = @_;
    my $obj = ($objects{$bucket} // {})->{$key};
    unless ($obj) {
        return send_s3_error_resp(404, "NoSuchKey", "The specified key does not exist.");
    }
    my $data = load_object_data($bucket, $key);
    unless (defined $data) {
        return send_s3_error_resp(500, "InternalError", "Failed to read object data");
    }

    my $total = length($data);

    if ($range && $range =~ /bytes=(\d+)-(\d*)/) {
        my $start = $1;
        if ($start >= $total) {
            return build_response(416, 'Range Not Satisfiable', {
                'Content-Range' => "bytes */$total",
                'Connection'    => 'close',
            }, '');
        }
        my $end = $2 ne '' ? $2 : $total - 1;
        $end = $total - 1 if $end >= $total;
        my $len = $end - $start + 1;
        my $slice = substr($data, $start, $len);
        return build_response(206, 'Partial Content', {
            'Content-Type'   => $obj->{content_type},
            'Content-Range'  => "bytes $start-$end/$total",
            'ETag'           => $obj->{etag},
            'Last-Modified'  => $obj->{last_modified},
            'Connection'     => 'close',
        }, $slice);
    }

    return build_response(200, 'OK', {
        'Content-Type'   => $obj->{content_type},
        'ETag'           => $obj->{etag},
        'Last-Modified'  => $obj->{last_modified},
        'Connection'     => 'close',
    }, $data);
}

sub head_object_resp {
    my ($bucket, $key) = @_;
    my $obj = ($objects{$bucket} // {})->{$key};
    unless ($obj) {
        return send_s3_error_resp(404, "NoSuchKey", "The specified key does not exist.");
    }
    return build_response(200, 'OK', {
        'Content-Type'   => $obj->{content_type},
        'Content-Length'  => $obj->{size},
        'ETag'           => $obj->{etag},
        'Last-Modified'  => $obj->{last_modified},
    }, '');
}

sub delete_object_resp {
    my ($bucket, $key) = @_;
    if ($objects{$bucket}) {
        delete $objects{$bucket}{$key};
        if ($USE_DISK) {
            unlink(disk_path($bucket, $key));
        }
    }
    fire_bucket_notification($bucket, $key, 'ObjectRemoved:Delete', 0, '');
    return build_response(204, 'No Content', {}, '');
}

sub list_buckets_resp {
    my $xml = qq{<?xml version="1.0" encoding="UTF-8"?>
<ListAllMyBucketsResult>
  <Owner>
    <ID>mini-aws</ID>
    <DisplayName>mini-aws</DisplayName>
  </Owner>
  <Buckets>};
    for my $bucket (sort keys %objects) {
        $xml .= qq{
    <Bucket>
      <Name>$bucket</Name>
      <CreationDate>} . amz_date() . qq{</CreationDate>
    </Bucket>};
    }
    $xml .= "\n  </Buckets>\n</ListAllMyBucketsResult>";
    return build_response(200, 'OK', { 'Content-Type' => 'application/xml' }, $xml);
}

sub list_objects_resp {
    my ($bucket, $query) = @_;
    my %params;
    for my $pair (split /&/, $query) {
        my ($k, $v) = split /=/, $pair, 2;
        $params{$k} = $v // '' if defined $k;
    }
    my $prefix    = $params{prefix}    // '';
    my $delimiter = $params{delimiter} // '';
    my $max_keys  = $params{'max-keys'} // 1000;

    my $bucket_data = $objects{$bucket} // {};
    my @all_keys = sort grep { $_ =~ /^\Q$prefix\E/ } keys %$bucket_data;

    my @contents;
    my %common_prefixes;
    for my $key (@all_keys) {
        if ($delimiter ne '' && (my $rest = substr($key, length($prefix))) =~ /\Q$delimiter\E/) {
            my $idx = index($rest, $delimiter);
            my $cp = $prefix . substr($rest, 0, $idx + length($delimiter));
            $common_prefixes{$cp} = 1;
        } else {
            push @contents, $key;
        }
    }

    @contents = splice(@contents, 0, $max_keys);
    my $is_truncated = (@all_keys > $max_keys) ? 'true' : 'false';

    my $xml = qq{<?xml version="1.0" encoding="UTF-8"?>
<ListBucketResult>
  <Name>$bucket</Name>
  <Prefix>$prefix</Prefix>
  <Delimiter>$delimiter</Delimiter>
  <MaxKeys>$max_keys</MaxKeys>
  <IsTruncated>$is_truncated</IsTruncated>};

    for my $key (@contents) {
        my $obj  = $bucket_data->{$key};
        my $size = $obj->{size};
        my $etag = $obj->{etag};
        my $date = amz_date();
        $xml .= qq{
  <Contents>
    <Key>$key</Key>
    <LastModified>$date</LastModified>
    <ETag>$etag</ETag>
    <Size>$size</Size>
  </Contents>};
    }

    for my $cp (sort keys %common_prefixes) {
        $xml .= qq{
  <CommonPrefixes>
    <Prefix>$cp</Prefix>
  </CommonPrefixes>};
    }

    $xml .= "\n</ListBucketResult>";
    return build_response(200, 'OK', { 'Content-Type' => 'application/xml' }, $xml);
}

sub url_decode {
    my $s = shift;
    return '' unless defined $s;
    $s =~ s/\+/ /g;
    $s =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/ge;
    return $s;
}

sub parse_form {
    my $s = shift;
    my %h;
    return %h unless defined $s && length $s;
    for my $pair (split /&/, $s) {
        my ($k, $v) = split /=/, $pair, 2;
        next unless defined $k && length $k;
        $h{url_decode($k)} = url_decode($v);
    }
    return %h;
}

sub json_escape_string {
    my $s = shift;
    $s =~ s/\\/\\\\/g;
    $s =~ s/"/\\"/g;
    $s =~ s/\x08/\\b/g;
    $s =~ s/\f/\\f/g;
    $s =~ s/\n/\\n/g;
    $s =~ s/\r/\\r/g;
    $s =~ s/\t/\\t/g;
    $s =~ s/([\x00-\x1f])/sprintf("\\u%04x", ord($1))/ge;
    return $s;
}

sub sqs_receive_resp {
    my ($account, $queue, $query) = @_;
    my $qname = "$account/$queue";
    my %params = parse_form($query);
    my $max = int($params{MaxNumberOfMessages} // 1) || 1;
    my $q = $sqs_queues{$qname} //= [];

    my @msgs;
    for my $m (@$q) {
        last if @msgs >= $max;
        next if $m->{invisible};
        $m->{invisible} = 1;
        push @msgs, $m;
    }

    my $msgs_json;
    if (@msgs) {
        my @parts;
        for my $m (@msgs) {
            my $body_enc = json_escape_string($m->{Body});
            my $md5 = md5_hex($m->{Body});
            push @parts, qq({"MessageId":"$m->{MessageId}","ReceiptHandle":"$m->{ReceiptHandle}","MD5OfBody":"$md5","Body":"$body_enc"});
        }
        $msgs_json = '[' . join(',', @parts) . ']';
    } else {
        $msgs_json = 'null';
    }
    my $json = qq({"ReceiveMessageResponse":{"ReceiveMessageResult":{"messages":$msgs_json},"ResponseMetadata":{"RequestId":"mini-aws"}}});
    return build_response(200, 'OK', { 'Content-Type' => 'application/json' }, $json);
}

sub sqs_delete_resp {
    my ($account, $queue, $body) = @_;
    my $qname = "$account/$queue";
    my %params = parse_form($body);
    my $rh = $params{ReceiptHandle};
    if (defined $rh && $sqs_queues{$qname}) {
        @{$sqs_queues{$qname}} = grep { $_->{ReceiptHandle} ne $rh } @{$sqs_queues{$qname}};
    }
    my $json = qq({"DeleteMessageResponse":{"ResponseMetadata":{"RequestId":"mini-aws"}}});
    return build_response(200, 'OK', { 'Content-Type' => 'application/json' }, $json);
}

sub sqs_add_message {
    my ($qname, $msg_body) = @_;
    my $rh  = "rh-" . ($nextReceipt++);
    my $mid = "mid-" . ($nextMsgId++);
    push @{$sqs_queues{$qname} //= []}, {
        ReceiptHandle => $rh,
        MessageId     => $mid,
        Body          => $msg_body,
        invisible     => 0,
    };
    return ($rh, $mid);
}

sub sqs_send_resp {
    my ($account, $queue, $body) = @_;
    my $qname = "$account/$queue";
    my %params = parse_form($body);
    my $msg_body = $params{MessageBody} // '';
    my ($rh, $mid) = sqs_add_message($qname, $msg_body);
    my $md5 = md5_hex($msg_body);
    my $json = qq({"SendMessageResponse":{"SendMessageResult":{"MessageId":"$mid","MD5OfMessageBody":"$md5"},"ResponseMetadata":{"RequestId":"mini-aws"}}});
    return build_response(200, 'OK', { 'Content-Type' => 'application/json' }, $json);
}

sub sqs_enqueue_resp {
    my ($account, $queue, $query, $body) = @_;
    my $qname = "$account/$queue";
    my %qp = parse_form($query);

    my $msg_body;
    if (defined $body && length $body) {
        $msg_body = $body;
    } else {
        my $bucket = $qp{bucket} // '';
        my $key    = $qp{key}    // '';
        my $b_esc  = json_escape_string($bucket);
        my $k_esc  = json_escape_string($key);
        $msg_body = qq({"Records":[{"s3":{"bucket":{"name":"$b_esc"},"object":{"key":"$k_esc"}}}]});
    }
    my ($rh, $mid) = sqs_add_message($qname, $msg_body);
    my $json = qq({"MessageId":"$mid","ReceiptHandle":"$rh","Queue":"$qname"});
    return build_response(200, 'OK', { 'Content-Type' => 'application/json' }, $json);
}

# Accept a flexible "ARN":
#   arn:aws:sqs:region:account:queue  -> account/queue
#   account/queue                     -> account/queue
#   queue                             -> mini-aws/queue
sub parse_queue_arn {
    my $a = shift;
    return undef unless defined $a && length $a;
    if ($a =~ /^arn:[^:]*:sqs:[^:]*:([^:]+):([^:]+)$/) {
        return "$1/$2";
    }
    if ($a =~ m{^([^/]+)/([^/]+)$}) {
        return "$1/$2";
    }
    return "mini-aws/$a";
}

sub put_bucket_notification_resp {
    my ($bucket, $body) = @_;
    my @configs;

    # Parse each <QueueConfiguration>...</QueueConfiguration> block.
    while ($body =~ m{<QueueConfiguration\b[^>]*>(.*?)</QueueConfiguration>}gs) {
        my $block = $1;
        my $arn;
        # AWS uses <Queue>, some SDKs use <QueueArn>
        if    ($block =~ m{<QueueArn>\s*([^<]+?)\s*</QueueArn>}) { $arn = $1; }
        elsif ($block =~ m{<Queue>\s*([^<]+?)\s*</Queue>})       { $arn = $1; }
        next unless defined $arn;
        my $qname = parse_queue_arn($arn);
        next unless defined $qname;
        my @events;
        while ($block =~ m{<Event>\s*([^<]+?)\s*</Event>}g) {
            push @events, $1;
        }
        @events = ('s3:ObjectCreated:*') unless @events;
        push @configs, { queue => $qname, events => \@events };
    }

    if (@configs) {
        $bucket_notifications{$bucket} = \@configs;
    } else {
        delete $bucket_notifications{$bucket};
    }
    return build_response(200, 'OK', {}, '');
}

sub get_bucket_notification_resp {
    my ($bucket) = @_;
    my $configs = $bucket_notifications{$bucket} // [];
    my $xml = qq{<?xml version="1.0" encoding="UTF-8"?>\n<NotificationConfiguration>};
    for my $c (@$configs) {
        $xml .= qq{\n  <QueueConfiguration>\n    <Queue>$c->{queue}</Queue>};
        for my $e (@{$c->{events}}) {
            $xml .= qq{\n    <Event>$e</Event>};
        }
        $xml .= qq{\n  </QueueConfiguration>};
    }
    $xml .= qq{\n</NotificationConfiguration>};
    return build_response(200, 'OK', { 'Content-Type' => 'application/xml' }, $xml);
}

sub event_matches {
    my ($pattern, $name) = @_;
    # name is e.g. ObjectCreated:Put, pattern like s3:ObjectCreated:* or s3:ObjectCreated:Put
    $pattern =~ s/^s3://;
    return 1 if $pattern eq '*' || $pattern eq $name;
    if ($pattern =~ /^(.*):\*$/) {
        my $prefix = $1;
        return 1 if $name =~ /^\Q$prefix\E:/;
    }
    return 0;
}

sub fire_bucket_notification {
    my ($bucket, $key, $event_name, $size, $etag) = @_;
    my $configs = $bucket_notifications{$bucket} // return;
    my $b_esc = json_escape_string($bucket);
    my $k_esc = json_escape_string($key);
    my $e_esc = json_escape_string("s3:$event_name");
    my $etag_clean = $etag;
    $etag_clean =~ s/^"|"$//g if defined $etag_clean;
    $etag_clean //= '';
    my $ts = strftime("%Y-%m-%dT%H:%M:%S.000Z", gmtime());
    my $msg_body = qq({"Records":[{"eventVersion":"2.1","eventSource":"aws:s3","eventTime":"$ts","eventName":"$e_esc","s3":{"bucket":{"name":"$b_esc"},"object":{"key":"$k_esc","size":$size,"eTag":"$etag_clean"}}}]});

    for my $c (@$configs) {
        my $matched = 0;
        for my $ev (@{$c->{events}}) {
            if (event_matches($ev, $event_name)) { $matched = 1; last; }
        }
        next unless $matched;
        sqs_add_message($c->{queue}, $msg_body);
        print "  notify -> $c->{queue}: $event_name $bucket/$key\n" if $debug;
    }
}

sub send_s3_error_resp {
    my ($status, $code, $message) = @_;
    my %status_text = (404 => 'Not Found', 500 => 'Internal Server Error', 501 => 'Not Implemented', 416 => 'Range Not Satisfiable');
    my $xml = qq{<?xml version="1.0" encoding="UTF-8"?>
<Error>
  <Code>$code</Code>
  <Message>$message</Message>
</Error>};
    return build_response($status, $status_text{$status} // 'Error', { 'Content-Type' => 'application/xml' }, $xml);
}
