-- From https://www.alienvault.com/open-threat-exchange/blog/luhn-checksum-algorithm-lua-implementation
function luhn_checksum(card)
    local num = 0
    local nDigits = card:len()
    local odd = nDigits & 1

    for count = 0,nDigits-1 do
        digit = tonumber(string.sub(card, count+1,count+1))
        if ((count & 1) ~ odd) == 0 then
                digit = digit * 2
        end

        if digit > 9 then
                digit = digit - 9
        end

        num = num + digit
    end

    return ((num % 10) == 0)
end

function cc_http_body(session, data)
    local matched, match = data:pcre_match(cc)

    if (matched and luhn_checksum(match)) then
        session:add_tag("has-credit-card")
        return -1
    end
end

-- From https://gist.github.com/kevinsearle/4213783
cc = MolochData.pcre_create("(?:4[0-9]{12}(?:[0-9]{3})?|5[1-5][0-9]{14}|6011[0-9]{12}|622((12[6-9]|1[3-9][0-9])|([2-8][0-9][0-9])|(9(([0-1][0-9])|(2[0-5]))))[0-9]{10}|64[4-9][0-9]{13}|65[0-9]{14}|3(?:0[0-5]|[68][0-9])[0-9]{11}|3[47][0-9]{13})")
MolochSession.register_body_feed("http", "cc_http_body")

