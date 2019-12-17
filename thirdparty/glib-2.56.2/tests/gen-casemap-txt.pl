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

# gen-casemap-test.pl - Generate test cases for case mapping from Unicode data.
# See http://www.unicode.org/Public/UNIDATA/UnicodeCharacterDatabase.html
# I consider the output of this program to be unrestricted.  Use it as
# you will.

require 5.006;
use utf8;

if (@ARGV != 3) {
    $0 =~ s@.*/@@;
    die "Usage: $0 UNICODE-VERSION UnicodeData.txt SpecialCasing.txt\n";
}
 
use vars qw($CODE $NAME $CATEGORY $COMBINING_CLASSES $BIDI_CATEGORY $DECOMPOSITION $DECIMAL_VALUE $DIGIT_VALUE $NUMERIC_VALUE $MIRRORED $OLD_NAME $COMMENT $UPPER $LOWER $TITLE $BREAK_CODE $BREAK_CATEGORY $BREAK_NAME $CASE_CODE $CASE_LOWER $CASE_TITLE $CASE_UPPER $CASE_CONDITION);

# Names of fields in Unicode data table.
$CODE = 0;
$NAME = 1;
$CATEGORY = 2;
$COMBINING_CLASSES = 3;
$BIDI_CATEGORY = 4;
$DECOMPOSITION = 5;
$DECIMAL_VALUE = 6;
$DIGIT_VALUE = 7;
$NUMERIC_VALUE = 8;
$MIRRORED = 9;
$OLD_NAME = 10;
$COMMENT = 11;
$UPPER = 12;
$LOWER = 13;
$TITLE = 14;

# Names of fields in the SpecialCasing table
$CASE_CODE = 0;
$CASE_LOWER = 1;
$CASE_TITLE = 2;
$CASE_UPPER = 3;
$CASE_CONDITION = 4;

my @upper;
my @title;
my @lower;

binmode STDOUT, ":utf8";
open (INPUT, "< $ARGV[1]") || exit 1;

$last_code = -1;
while (<INPUT>)
{
    chop;
    @fields = split (';', $_, 30);
    if ($#fields != 14)
    {
	printf STDERR ("Entry for $fields[$CODE] has wrong number of fields (%d)\n", $#fields);
    }

    $code = hex ($fields[$CODE]);

    if ($code > $last_code + 1)
    {
	# Found a gap.
	if ($fields[$NAME] =~ /Last>/)
	{
	    # Fill the gap with the last character read,
            # since this was a range specified in the char database
	    @gfields = @fields;
	}
	else
	{
	    # The gap represents undefined characters.  Only the type
	    # matters.
	    @gfields = ('', '', 'Cn', '0', '', '', '', '', '', '', '',
			'', '', '', '');
	}
	for (++$last_code; $last_code < $code; ++$last_code)
	{
	    $gfields{$CODE} = sprintf ("%04x", $last_code);
	    &process_one ($last_code, @gfields);
	}
    }
    &process_one ($code, @fields);
    $last_code = $code;
}

close INPUT;

open (INPUT, "< $ARGV[2]") || exit 1;

while (<INPUT>)
{
    my $code;
    
    chop;

    next if /^#/;
    next if /^\s*$/;

    s/\s*#.*//;

    @fields = split ('\s*;\s*', $_, 30);

    $raw_code = $fields[$CASE_CODE];
    $code = hex ($raw_code);

    if ($#fields != 4 && $#fields != 5)
    {
	printf STDERR ("Entry for $raw_code has wrong number of fields (%d)\n", $#fields);
	next;
    }

    if (defined $fields[5]) {
	# Ignore conditional special cases - we'll handle them manually
	next;
    }

    $upper[$code] = &make_hex ($fields[$CASE_UPPER]);
    $lower[$code] = &make_hex ($fields[$CASE_LOWER]);
    $title[$code] = &make_hex ($fields[$CASE_TITLE]);
}

close INPUT;

print <<EOT;
# Test cases generated from Unicode $ARGV[0] data
# by gen-case-tests.pl. Do not edit.
#
# Some special hand crafted tests
#
tr_TR\ti\ti\t\x{0130}\t\x{0130}\t# i => LATIN CAPITAL LETTER I WITH DOT ABOVE
tr_TR\tI\t\x{0131}\tI\tI\t# I => LATIN SMALL LETTER DOTLESS I
tr_TR\tI\x{0307}\ti\tI\x{0307}\tI\x{0307}\t# I => LATIN SMALL LETTER DOTLESS I
tr_TR.UTF-8\ti\ti\t\x{0130}\t\x{0130}\t# i => LATIN CAPITAL LETTER I WITH DOT ABOVE
tr_TR.UTF-8\tI\t\x{0131}\tI\tI\t# I => LATIN SMALL LETTER DOTLESS I
tr_TR.UTF-8\tI\x{0307}\ti\tI\x{0307}\tI\x{0307}\t# I => LATIN SMALL LETTER DOTLESS I
# Test reordering of YPOGEGRAMMENI across other accents
\t\x{03b1}\x{0345}\x{0314}\t\x{03b1}\x{0345}\x{314}\t\x{0391}\x{0345}\x{0314}\t\x{0391}\x{0314}\x{0399}\t
\t\x{03b1}\x{0314}\x{0345}\t\x{03b1}\x{314}\x{0345}\t\x{0391}\x{0314}\x{0345}\t\x{0391}\x{0314}\x{0399}\t
# Handling of final and nonfinal sigma
	ΜΆΙΟΣ 	μάιος 	Μάιος 	ΜΆΙΟΣ 	
	ΜΆΙΟΣ	μάιος	Μάιος	ΜΆΙΟΣ	
	ΣΙΓΜΑ	σιγμα	Σιγμα	ΣΙΓΜΑ	
# Lithuanian rule of i followed by letter with dot. Not at all sure
# about the titlecase part here
lt_LT\ti\x{117}\ti\x{117}\tIe\tIE\t
lt_LT\tie\x{307}\tie\x{307}\tIe\tIE\t
lt_LT\t\x{00cc}\ti\x{0307}\x{0300}\t\x{00cc}\t\x{00cc}\t # LATIN CAPITAL LETTER I WITH GRAVE
lt_LT\t\x{00CD}\ti\x{0307}\x{0301}\t\x{00CD}\t\x{00CD}\t # LATIN CAPITAL LETTER I WITH ACUTE
lt_LT\t\x{0128}\ti\x{0307}\x{0303}\t\x{0128}\t\x{0128}\t # LATIN CAPITAL LETTER I WITH TILDE
lt_LT\tI\x{0301}\ti\x{0307}\x{0301}\tI\x{0301}\tI\x{0301}\t # LATIN CAPITAL LETTER I (with acute accent)
lt_LT\tI\x{0300}\ti\x{0307}\x{0300}\tI\x{0300}\tI\x{0300}\t # LATIN CAPITAL LETTER I (with grave accent)
lt_LT\tI\x{0303}\ti\x{0307}\x{0303}\tI\x{0303}\tI\x{0303}\t # LATIN CAPITAL LETTER I (with tilde above)
lt_LT\tI\x{0328}\x{0301}\ti\x{0307}\x{0328}\x{0301}\tI\x{0328}\x{0301}\tI\x{0328}\x{0301}\t # LATIN CAPITAL LETTER I (with ogonek and acute accent)
lt_LT\tJ\x{0301}\tj\x{0307}\x{0301}\tJ\x{0301}\tJ\x{0301}\t # LATIN CAPITAL LETTER J (with acute accent)
lt_LT\t\x{012e}\x{0301}\t\x{012f}\x{0307}\x{0301}\t\x{012e}\x{0301}\t\x{012e}\x{0301}\t # LATIN CAPITAL LETTER I WITH OGONEK (with acute accent)
lt_LT.UTF-8\ti\x{117}\ti\x{117}\tIe\tIE\t
lt_LT.UTF-8\tie\x{307}\tie\x{307}\tIe\tIE\t
lt_LT.UTF-8\t\x{00cc}\ti\x{0307}\x{0300}\t\x{00cc}\t\x{00cc}\t # LATIN CAPITAL LETTER I WITH GRAVE
lt_LT.UTF-8\t\x{00CD}\ti\x{0307}\x{0301}\t\x{00CD}\t\x{00CD}\t # LATIN CAPITAL LETTER I WITH ACUTE
lt_LT.UTF-8\t\x{0128}\ti\x{0307}\x{0303}\t\x{0128}\t\x{0128}\t # LATIN CAPITAL LETTER I WITH TILDE
lt_LT.UTF-8\tI\x{0301}\ti\x{0307}\x{0301}\tI\x{0301}\tI\x{0301}\t # LATIN CAPITAL LETTER I (with acute accent)
lt_LT.UTF-8\tI\x{0300}\ti\x{0307}\x{0300}\tI\x{0300}\tI\x{0300}\t # LATIN CAPITAL LETTER I (with grave accent)
lt_LT.UTF-8\tI\x{0303}\ti\x{0307}\x{0303}\tI\x{0303}\tI\x{0303}\t # LATIN CAPITAL LETTER I (with tilde above)
lt_LT.UTF-8\tI\x{0328}\x{0301}\ti\x{0307}\x{0328}\x{0301}\tI\x{0328}\x{0301}\tI\x{0328}\x{0301}\t # LATIN CAPITAL LETTER I (with ogonek and acute accent)
lt_LT.UTF-8\tJ\x{0301}\tj\x{0307}\x{0301}\tJ\x{0301}\tJ\x{0301}\t # LATIN CAPITAL LETTER J (with acute accent)
lt_LT.UTF-8\t\x{012e}\x{0301}\t\x{012f}\x{0307}\x{0301}\t\x{012e}\x{0301}\t\x{012e}\x{0301}\t # LATIN CAPITAL LETTER I WITH OGONEK (with acute accent)
# Special case not at initial position
\ta\x{fb04}\ta\x{fb04}\tAffl\tAFFL\t# FB04
#
# Now the automatic tests
#
EOT
&print_tests;

exit 0;

# Process a single character.
sub process_one
{
    my ($code, @fields) = @_;

    my $type =  $fields[$CATEGORY];
    if ($type eq 'Ll')
    {
	$upper[$code] = make_hex ($fields[$UPPER]);
	$lower[$code] = pack ("U", $code);
	$title[$code] = make_hex ($fields[$TITLE]);
    }
    elsif ($type eq 'Lu')
    {
	$lower[$code] = make_hex ($fields[$LOWER]);
	$upper[$code] = pack ("U", $code);
	$title[$code] = make_hex ($fields[$TITLE]);
    }

    if ($type eq 'Lt')
    {
	$upper[$code] = make_hex ($fields[$UPPER]);
	$lower[$code] = pack ("U", hex ($fields[$LOWER]));
	$title[$code] = make_hex ($fields[$LOWER]);
    }
}

sub print_tests
{
    for ($i = 0; $i < 0x10ffff; $i++) {
	if ($i == 0x3A3) {
	    # Greek sigma needs special tests
	    next;
	}
	
	my $lower = $lower[$i];
	my $title = $title[$i];
	my $upper = $upper[$i];

	if (defined $upper || defined $lower || defined $title) {
	    printf "\t%s\t%s\t%s\t%s\t# %4X\n",
		    pack ("U", $i),
		    (defined $lower ? $lower : ""),
		    (defined $title ? $title : ""),
		    (defined $upper ? $upper : ""),
                    $i;
	}
    }
}

sub make_hex
{
    my $codes = shift;

    $codes =~ s/^\s+//;
    $codes =~ s/\s+$//;

    if ($codes eq "0" || $codes eq "") {
	return "";
    } else {
	return pack ("U*", map { hex ($_) } split /\s+/, $codes);
    }
}
