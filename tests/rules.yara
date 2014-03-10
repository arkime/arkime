rule Pop3Yara: tag1 tag2
{
    strings:
        $o = " POP3 "

    condition:
        $o in (0 .. 50)
}
