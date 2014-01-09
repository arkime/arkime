rule Pop3Yara
{
    strings:
        $o = " POP3 "

    condition:
        $o in (0 .. 50)
}
