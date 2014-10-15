#!/usr/bin/perl
# curl http://www.iana.org/assignments/tls-parameters/tls-parameters-4.csv | ./tls-cipher.h.pl > tls-cipher.h
use strict;

my @row00; 
my @rowC0;

while (<>) {
    my @row = split(",");
    next if ($row[1] =~ /-/);
    if($row[0] eq '"0x00') {
        $row00[hex($row[1])] = uc($row[2]);
    } elsif($row[0] eq '"0xC0') { 
        $rowC0[hex($row[1])] = uc($row[2]);
    }
}

print <<EOF;
#ifndef MOLOCH_TLS_CIPHER_H
#define MOLOCH_TLS_CIPHER_H
EOF

print "static char *cipher00[256] = {\n";
for (my $i = 0 ; $i < 256; $i++) {
    print $row00[$i]? '"' . $row00[$i] . '"': "NULL";
    print ",\n" if ($i != 255);
}
print "};\n\n";
print "static char *cipherC0[256] = {\n";
for (my $i = 0 ; $i < 256; $i++) {
    print $rowC0[$i]? '"' . $rowC0[$i] . '"': "NULL";
    print ",\n" if ($i != 255);
}
print "};\n\n";
print <<EOF;
#endif
EOF

