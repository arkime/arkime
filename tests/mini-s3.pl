#!/usr/bin/env perl
#
# SPDX-License-Identifier: Apache-2.0
#
# Mini S3 server for testing. Stores everything in memory, ignores auth.
# Non-blocking I/O with per-connection buffering to prevent one slow
# client from starving others.
# Supports: CreateMultipartUpload, UploadPart, CompleteMultipartUpload,
#           PutObject, GetObject, DeleteObject, HeadObject,
#           ListBuckets, ListObjects

use strict;
use warnings;
$SIG{INT} = sub { exit 0; };
use IO::Socket::INET;
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
my $DISK_DIR = "/tmp/mini-s3";

# Storage: $objects{$bucket}{$key} = { data => ..., content_type => ..., etag => ..., size => ..., last_modified => ... }
my %objects;

# Multipart: $uploads{$uploadId} = { bucket => ..., key => ..., parts => { $partNum => { data => ..., etag => ... } } }
my %uploads;
my $nextUploadId = 1;

# Per-connection state
my %conn_buf;    # fileno => raw read buffer
my %conn_state;  # fileno => { method, uri, query, headers, content_length, body, headers_done }

my $server = IO::Socket::INET->new(
    LocalPort => $port,
    Proto     => 'tcp',
    Listen    => 128,
    ReuseAddr => 1,
) or die "Cannot start server on port $port: $!\n";

my $flags = fcntl($server, F_GETFL, 0);
fcntl($server, F_SETFL, $flags | O_NONBLOCK);

print "Mini S3 listening on port $port\n";

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
    my $ts = strftime("%H:%M:%S", localtime);
    print "$ts S3: $method $uri" . ($query ne '' ? "?$query" : "") . " body:$body_size" . ($range ne '' ? " Range:$range" : "") . "\n" if $debug;

    if ($uri eq '/_shutdown') {
        return '_shutdown';
    }

    my ($bucket, $key) = parse_path($uri);
    print "  bucket=$bucket key=" . ($key // '') . "\n" if ($debug && defined $bucket);

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
    return build_response(204, 'No Content', {}, '');
}

sub list_buckets_resp {
    my $xml = qq{<?xml version="1.0" encoding="UTF-8"?>
<ListAllMyBucketsResult>
  <Owner>
    <ID>mini-s3</ID>
    <DisplayName>mini-s3</DisplayName>
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
