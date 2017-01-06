#!/usr/bin/perl
# curl http://www.iana.org/assignments/tls-parameters/tls-parameters-4.csv | ./tls-cipher.h.pl > tls-cipher.h
use strict;
use Data::Dumper;

my @rows; 

for (my $i = 0; $i < 256; $i++) {
    @rows[$i] = [];
}

# https://tools.ietf.org/html/draft-agl-tls-chacha20poly1305-04
$rows[0xcc][0x13] = "TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256";
$rows[0xcc][0x14] = "TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256";
$rows[0xcc][0x15] = "TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256";

# https://tools.ietf.org/html/draft-josefsson-salsa20-tls-04
$rows[0xe4][0x10] = "TLS_RSA_WITH_ESTREAM_SALSA20_SHA1";
$rows[0xe4][0x11] = "TLS_RSA_WITH_SALSA20_SHA1";
$rows[0xe4][0x12] = "TLS_ECDHE_RSA_WITH_ESTREAM_SALSA20_SHA1";
$rows[0xe4][0x13] = "TLS_ECDHE_RSA_WITH_SALSA20_SHA1";
$rows[0xe4][0x14] = "TLS_ECDHE_ECDSA_WITH_ESTREAM_SALSA20_SHA1";
$rows[0xe4][0x15] = "TLS_ECDHE_ECDSA_WITH_SALSA20_SHA1";
$rows[0xe4][0x16] = "TLS_PSK_WITH_ESTREAM_SALSA20_SHA1";
$rows[0xe4][0x17] = "TLS_PSK_WITH_SALSA20_SHA1";
$rows[0xe4][0x18] = "TLS_ECDHE_PSK_WITH_ESTREAM_SALSA20_SHA1";
$rows[0xe4][0x19] = "TLS_ECDHE_PSK_WITH_SALSA20_SHA1";
$rows[0xe4][0x1A] = "TLS_RSA_PSK_WITH_ESTREAM_SALSA20_SHA1";
$rows[0xe4][0x1B] = "TLS_RSA_PSK_WITH_SALSA20_SHA1";
$rows[0xe4][0x1C] = "TLS_DHE_PSK_WITH_ESTREAM_SALSA20_SHA1";
$rows[0xe4][0x1D] = "TLS_DHE_PSK_WITH_SALSA20_SHA1";
$rows[0xe4][0x1E] = "TLS_DHE_RSA_WITH_ESTREAM_SALSA20_SHA1";
$rows[0xe4][0x1F] = "TLS_DHE_RSA_WITH_SALSA20_SHA1";

# https://tools.ietf.org/html/draft-ietf-tls-tls13-18
$rows[0x13][0x01] = "TLS_AES_128_GCM_SHA256";
$rows[0x13][0x02] = "TLS_AES_256_GCM_SHA384";
$rows[0x13][0x03] = "TLS_CHACHA20_POLY1305_SHA256";
$rows[0x13][0x04] = "TLS_AES_128_CCM_SHA256";
$rows[0x13][0x05] = "TLS_AES_128_CCM_8_SHA256";

while (<>) {
    my @row = split(",");
    next if ($row[1] =~ /-/);
    next if ($row[0] !~ /^"0x([[:xdigit:]][[:xdigit:]])/);
    $row[0] = $1;
    next if ($row[1] !~ /^0x([[:xdigit:]][[:xdigit:]])/);
    $row[1] = $1;

    $rows[hex($row[0])][hex($row[1])] = uc($row[2]);
}

print <<EOF;
#ifndef MOLOCH_TLS_CIPHER_H
#define MOLOCH_TLS_CIPHER_H
EOF

for (my $i = 0; $i < 256; $i++) {
    if (scalar @{$rows[$i]} > 0) {
        printf "static char *ciphers_%02x[256] = {\n", $i;
        for (my $j = 0 ; $j < 256; $j++) {
            print $rows[$i][$j]? '"' . $rows[$i][$j] . '"': "NULL";
            print ",\n" if ($j != 255);
        }
        print "};\n\n";
    }
}

print "static char *ciphers_null[256] = {";
for (my $j = 0 ; $j < 256; $j++) {
    print "NULL";
    print ", " if ($j != 255);
}
print "};\n\n";

print "static char **ciphers[256] = {\n";
for (my $i = 0; $i < 256; $i++) {
    if (scalar @{$rows[$i]} > 0) {
        printf "ciphers_%02x", $i;
    } else {
        print "ciphers_null";
    }
    print ",\n" if ($i != 255);
}
print "};\n\n";

print <<EOF;
#endif
EOF
