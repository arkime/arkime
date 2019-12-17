#! /usr/bin/perl -w

#    Copyright (C) 1998, 1999 Tom Tromey
#    Copyright (C) 2001 Red Hat Software

#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2, or (at your option)
#    any later version.

#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.

#    You should have received a copy of the GNU General Public License
#    along with this program; if not, see <http://www.gnu.org/licenses/>.

# gen-casefold-test.pl - Generate test cases for casefolding from Unicode data.
# See http://www.unicode.org/Public/UNIDATA/UnicodeCharacterDatabase.html
# Usage: 
# I consider the output of this program to be unrestricted.  Use it as
# you will.

require 5.006;

# Names of fields in the CaseFolding table
$FOLDING_CODE = 0;
$FOLDING_STATUS = 1;
$FOLDING_MAPPING = 2;

my $casefoldlen = 0;
my @casefold;

if (@ARGV != 2) {
    $0 =~ s@.*/@@;
    die "Usage: $0 UNICODE-VERSION  CaseFolding.txt\n";
}
 
print <<EOT;
# Test cases generated from Unicode $ARGV[0] data
# by gen-casefold-test.pl. Do not edit.
#
# Some special hand crafted tests
#
AaBbCc@@\taabbcc@@
#
# Now the automatic tests
#
EOT

binmode STDOUT, ":utf8";
open (INPUT, "< $ARGV[1]") || exit 1;

while (<INPUT>)
{
    chop;

    next if /^#/;
    next if /^\s*$/;

    s/\s*#.*//;

    my @fields = split ('\s*;\s*', $_, 30);

    my $raw_code = $fields[$FOLDING_CODE];
    my $code = hex ($raw_code);

    if ($#fields != 3)
    {
	printf STDERR ("Entry for $raw_code has wrong number of fields (%d)\n", $#fields);
	next;
    }

    # skip simple and Turkic mappings
    next if ($fields[$FOLDING_STATUS] =~ /^[ST]$/);

    @values = map { hex ($_) } split /\s+/, $fields[$FOLDING_MAPPING];
    printf ("%s\t%s\n", pack ("U", $code), pack ("U*", @values));
}

close INPUT;
