use Test::More tests => 92;
use Cwd;
use URI::Escape;
use MolochTest;
use strict;

my $pwd = getcwd() . "/pcap";
my $files = "(file=$pwd/smtp-subject-8859-b.pcap||file=$pwd/smtp-data-250.pcap||file=$pwd/smtp-originating.pcap||file=$pwd/smtp-zip.pcap||file=$pwd/smtp-subject-multi-nospace.pcap||file=$pwd/smtp-subject-utf8-q.pcap)";

countTest(6, "date=-1&expression=" . uri_escape("$files&&protocols==smtp"));

# asn.email
    countTest(1, "date=-1&expression=" . uri_escape("$files&&asn.email==\"AS0001 Cool Beans!\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&asn.email==\"AS0001\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&asn.email==\"aS0001\""));

# ip.email
    countTest(2, "date=-1&expression=" . uri_escape("$files&&ip.email==10.0.0.4"));

# country.email
    countTest(1, "date=-1&expression=" . uri_escape("$files&&country.email==USA"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&country.email==usa"));

# rir.email
    countTest(1, "date=-1&expression=" . uri_escape("$files&&rir.email==ARIN"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&rir.email==arin"));

# host.email
    countTest(1, "date=-1&expression=" . uri_escape("$files&&host.email==xxxxxxxxxxxxxxxxxxxx.xxxxxxxxx.net"));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&host.email==Xxxxxxxxxxxxxxxxxxxx.xxxxxxxxx.net"));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&host.email.cnt==2"));

# email.bodymagic
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.bodymagic==\"application/zip\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&email.bodymagic==\"Application/zip\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.bodymagic.cnt==1"));

# email.dst
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.dst==\"xxxxxxxxx\@xxxxxxx.com\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.dst==\"Xxxxxxxxx\@xxxxxxx.com\""));
    countTest(5, "date=-1&expression=" . uri_escape("$files&&email.dst.cnt==1"));

# email.fn
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.fn==\"a.zip\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&email.fn==\"A.zip\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.fn.cnt==1"));

# email.has-header
    countTest(6, "date=-1&expression=" . uri_escape("$files&&email.has-header==\"to\""));
    countTest(6, "date=-1&expression=" . uri_escape("$files&&email.has-header==\"To\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.has-header.cnt==3"));

# email.bodymagic
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.bodymagic==\"application/zip\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&email.bodymagic==\"Application/zip\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.bodymagic.cnt==1"));

# email.md5
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.md5==\"5b153a606bea42005e1eedb5ddeabcf0\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&email.md5==\"5B153a606bea42005e1eedb5ddeabcf0\""));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&email.md5.cnt==1"));

# email.message-id
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.message-id==\"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\@xxxxxxxxx.net\""));
    countTest(0, "date=-1&expression=" . uri_escape("$files&&email.message-id==\"Xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\@xxxxxxxxx.net\""));
    countTest(4, "date=-1&expression=" . uri_escape("$files&&email.message-id.cnt==1"));

# email.bodymagic
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.mime-version==\"1.0 (Apple Message framework v1283)\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.mime-version==\"1.0 (Apple Message framework v1283)\""));
    countTest(5, "date=-1&expression=" . uri_escape("$files&&email.mime-version.cnt==1"));

# email.src
    countTest(2, "date=-1&expression=" . uri_escape("$files&&email.src==\"xxxxx\@xxx.net\""));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&email.src==\"Xxxxx\@xxx.net\""));
    countTest(6, "date=-1&expression=" . uri_escape("$files&&email.src.cnt==1"));

# email.x-mailer
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.x-mailer==\"Mutt/1.5.20 (2009-12-10)\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.x-mailer==\"mutt/1.5.20 (2009-12-10)\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&email.x-mailer==Mutt"));
    countTest(3, "date=-1&expression=" . uri_escape("$files&&email.x-mailer.cnt==1"));

# host.email
    countTest(1, "date=-1&expression=" . uri_escape("$files&&host.email==\"xxxxxxxxxxxxxxxxxxxx.xxxxxxxxx.net\""));
    countTest(1, "date=-1&expression=" . uri_escape("$files&&host.email==\"xxxxxxxxxxxxxxxxxxxx.xxxxxxxxx.net\""));
    countTest(2, "date=-1&expression=" . uri_escape("$files&&host.email.cnt==2"));
