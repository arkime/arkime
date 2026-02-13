#!/usr/bin/env perl
#
# SPDX-License-Identifier: Apache-2.0
#
# Mini S3 server for testing. Stores everything in memory, ignores auth.
# Supports: CreateMultipartUpload, UploadPart, CompleteMultipartUpload,
#           PutObject, GetObject, DeleteObject

use strict;
use warnings;
use HTTP::Daemon;
use HTTP::Status;
use Digest::MD5 qw(md5_hex);
use POSIX qw(strftime);
use File::Path qw(make_path);

my $port = $ARGV[0] || 9000;
my $USE_DISK = 0; # Set to 0 to store everything in memory
my $DISK_DIR = "/tmp/mini-s3";

# Storage: $objects{$bucket}{$key} = { data => ..., content_type => ..., etag => ... }
my %objects;

# Multipart: $uploads{$uploadId} = { bucket => ..., key => ..., parts => { $partNum => { data => ..., etag => ... } } }
my %uploads;
my $nextUploadId = 1;

my $daemon = HTTP::Daemon->new(
    LocalPort => $port,
    ReuseAddr => 1,
    ReusePort => 1,
) or die "Cannot start server on port $port: $!\n";

print "Mini S3 listening on port $port\n";

while (my $conn = $daemon->accept) {
    binmode($conn);
    while (my $req = $conn->get_request) {
        eval {
            handle_request($conn, $req);
        };
        if ($@) {
            warn "Error handling request: $@\n";
            $conn->send_error(500, "Internal Server Error");
        }
    }
    $conn->close;
}

sub handle_request {
    my ($conn, $req) = @_;
    my $method = $req->method;
    my $uri    = $req->uri->path;
    my $query  = $req->uri->query // '';

    my $range = $req->header('Range') // '';
    my $body_size = length($req->content // '');
    my $date = strftime("%Y-%m-%dT%H:%M:%S", gmtime());
    print "$date $method $uri" . ($query ne '' ? "?$query" : "") . " body:$body_size" . ($range ne '' ? " Range:$range" : "") . "\n";

    if ($uri eq '/_shutdown') {
        my $resp = HTTP::Response->new(200);
        $resp->header('Content-Length' => 0);
        $conn->send_response($resp);
        $conn->close;
        exit(0);
    }

    my ($bucket, $key) = parse_path($uri);
    print "  bucket=$bucket key=$key\n" if defined $bucket;

    if ($method eq 'POST' && $query =~ /uploads\b/ && !($query =~ /uploadId/)) {
        # CreateMultipartUpload
        create_multipart_upload($conn, $bucket, $key, $req);
    } elsif ($method eq 'PUT' && $query =~ /uploadId=([^&]+)/) {
        # UploadPart
        my $uploadId  = $1;
        $query =~ /partNumber=(\d+)/;
        my $partNum = $1;
        upload_part($conn, $uploadId, $partNum, $req);
    } elsif ($method eq 'POST' && $query =~ /uploadId=([^&]+)/) {
        # CompleteMultipartUpload
        complete_multipart_upload($conn, $1, $req);
    } elsif ($method eq 'GET' && !defined $bucket) {
        list_buckets($conn);
    } elsif ($method eq 'GET' && !defined $key && defined $bucket) {
        list_objects($conn, $bucket, $req, $query);
    } elsif ($method eq 'PUT' && defined $key) {
        put_object($conn, $bucket, $key, $req);
    } elsif ($method eq 'GET' && defined $key) {
        get_object($conn, $bucket, $key, $req);
    } elsif ($method eq 'DELETE' && defined $key) {
        delete_object($conn, $bucket, $key, $req);
    } elsif ($method eq 'HEAD' && defined $key) {
        head_object($conn, $bucket, $key, $req);
    } else {
        send_s3_error($conn, 501, "NotImplemented", "Not implemented: $method $uri?$query");
    }
}

sub parse_path {
    my ($path) = @_;
    # /bucket/key/parts...
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
    my $path = "$DISK_DIR/$bucket/$key";
    return $path;
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
    my ($conn, $bucket, $key, $req) = @_;
    my $uploadId = $nextUploadId++;
    $uploads{$uploadId} = {
        bucket => $bucket,
        key    => $key,
        parts  => {},
    };
    my $xml = qq{<?xml version="1.0" encoding="UTF-8"?>
<InitiateMultipartUploadResult>
  <Bucket>$bucket</Bucket>
  <Key>$key</Key>
  <UploadId>$uploadId</UploadId>
</InitiateMultipartUploadResult>};
    send_xml($conn, 200, $xml);
}

sub upload_part {
    my ($conn, $uploadId, $partNum, $req) = @_;
    my $upload = $uploads{$uploadId};
    unless ($upload) {
        send_s3_error($conn, 404, "NoSuchUpload", "Upload $uploadId not found");
        return;
    }
    my $data = $req->content_ref;
    my $etag = '"' . md5_hex($$data) . '"';
    $upload->{parts}{$partNum} = {
        data => $$data,
        etag => $etag,
    };
    my $resp = HTTP::Response->new(200);
    $resp->header('ETag' => $etag);
    $resp->header('Content-Length' => 0);
    $conn->send_response($resp);
}

sub complete_multipart_upload {
    my ($conn, $uploadId, $req) = @_;
    my $upload = $uploads{$uploadId};
    unless ($upload) {
        send_s3_error($conn, 404, "NoSuchUpload", "Upload $uploadId not found");
        return;
    }

    # Concatenate parts in order
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
    send_xml($conn, 200, $xml);
}

sub put_object {
    my ($conn, $bucket, $key, $req) = @_;
    my $data = ${$req->content_ref};
    my $etag = save_object($bucket, $key, $data, $req->header('Content-Type') // 'application/octet-stream');
    my $resp = HTTP::Response->new(200);
    $resp->header('ETag' => $etag);
    $resp->header('Content-Length' => 0);
    $conn->send_response($resp);
}

sub send_data {
    my ($conn, $status, $headers, $dataref, $offset, $length) = @_;
    $offset //= 0;
    $length //= length($$dataref) - $offset;

    my $header_str = "HTTP/1.1 $status " . HTTP::Status::status_message($status) . "\r\n";
    for my $k (keys %$headers) {
        $header_str .= "$k: $headers->{$k}\r\n";
    }
    $header_str .= "\r\n";
    $conn->print($header_str);

    my $chunk_size = 1024 * 1024;
    my $pos = $offset;
    my $end = $offset + $length;
    while ($pos < $end) {
        my $len = ($end - $pos < $chunk_size) ? $end - $pos : $chunk_size;
        $conn->print(substr($$dataref, $pos, $len));
        $pos += $len;
    }
}

sub get_object {
    my ($conn, $bucket, $key, $req) = @_;
    my $obj = ($objects{$bucket} // {})->{$key};
    unless ($obj) {
        send_s3_error($conn, 404, "NoSuchKey", "The specified key does not exist.");
        return;
    }

    my $data = load_object_data($bucket, $key);
    unless (defined $data) {
        send_s3_error($conn, 500, "InternalError", "Failed to read object data");
        return;
    }

    my $total = length($data);
    my $range = $req->header('Range');

    if ($range && $range =~ /bytes=(\d+)-(\d*)/) {
        my $start = $1;
        if ($start >= $total) {
            my $resp = HTTP::Response->new(416);
            $resp->header('Content-Range' => "bytes */$total");
            $resp->header('Content-Length' => 0);
            $conn->send_response($resp);
            return;
        }
        my $end = $2 ne '' ? $2 : $total - 1;
        $end = $total - 1 if $end >= $total;
        my $len = $end - $start + 1;
        send_data($conn, 206, {
            'Content-Type'   => $obj->{content_type},
            'Content-Length' => $len,
            'Content-Range'  => "bytes $start-$end/$total",
            'ETag'           => $obj->{etag},
            'Last-Modified'  => $obj->{last_modified},
            'Connection'     => 'close',
        }, \$data, $start, $len);
        return;
    }

    send_data($conn, 200, {
        'Content-Type'   => $obj->{content_type},
        'Content-Length' => $total,
        'ETag'           => $obj->{etag},
        'Last-Modified'  => $obj->{last_modified},
        'Connection'     => 'close',
    }, \$data);
}

sub head_object {
    my ($conn, $bucket, $key, $req) = @_;
    my $obj = ($objects{$bucket} // {})->{$key};
    unless ($obj) {
        send_s3_error($conn, 404, "NoSuchKey", "The specified key does not exist.");
        return;
    }
    my $resp = HTTP::Response->new(200);
    $resp->header('Content-Type'   => $obj->{content_type});
    $resp->header('Content-Length' => $obj->{size});
    $resp->header('ETag'           => $obj->{etag});
    $resp->header('Last-Modified'  => $obj->{last_modified});
    $conn->send_response($resp);
}

sub list_buckets {
    my ($conn) = @_;
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
    send_xml($conn, 200, $xml);
}

sub list_objects {
    my ($conn, $bucket, $req, $query) = @_;
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
    send_xml($conn, 200, $xml);
}

sub delete_object {
    my ($conn, $bucket, $key, $req) = @_;
    if ($objects{$bucket}) {
        delete $objects{$bucket}{$key};
        if ($USE_DISK) {
            unlink(disk_path($bucket, $key));
        }
    }
    my $resp = HTTP::Response->new(204);
    $resp->header('Content-Length' => 0);
    $conn->send_response($resp);
}

sub send_xml {
    my ($conn, $status, $xml) = @_;
    my $resp = HTTP::Response->new($status);
    $resp->header('Content-Type'   => 'application/xml');
    $resp->header('Content-Length' => length($xml));
    $resp->content($xml);
    $conn->send_response($resp);
}

sub send_s3_error {
    my ($conn, $status, $code, $message) = @_;
    my $xml = qq{<?xml version="1.0" encoding="UTF-8"?>
<Error>
  <Code>$code</Code>
  <Message>$message</Message>
</Error>};
    send_xml($conn, $status, $xml);
}
