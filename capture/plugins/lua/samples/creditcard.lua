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

--[[
Find credit cards by doing a quick match, luhn check, and then better match.
Set possible-credit-card if one match found, set has-credit-cards if 3 matches found
--]]
function cc_http_body(session, data)
    local matched, match = data:pcre_match(fastCreditCard)

    if matched then
        match = match:gsub("[ -]", "")
	if luhn_checksum(match) then
	    local dmatch = MolochData.new(match);
	    if dmatch:pcre_ismatch(fullCreditCard) then
                local t = session:table()
                if (t.cccount == nil) then
                    t.cccount = 1
                    session:add_tag("possible-credit-card")
                else
                    t.cccount = t.cccount + 1
                    if (t.cccount > 2) then
                        session:add_tag("has-credit-cards")
                        return -1
                    end
                end
	    end
	end
    end
end

-- From http://www.regular-expressions.info/creditcard.html
-- [[ ]] means ignore escapes in lua
fullCreditCard = MolochData.pcre_create([[^(?:4[0-9]{12}(?:[0-9]{3})?(?#Visa
)|5[1-5][0-9]{14}(?#MasterCard
)|3[47][0-9]{13}(?#American Express
)|3(?:0[0-5]|[68][0-9])[0-9]{11}(?#Diners Club
)|6(?:011|5[0-9]{2})[0-9]{12}(?#Discover
)|(?:2131|1800|35\d{3})\d{11}(?#JCB
))$]])

fastCreditCard = MolochData.pcre_create([[\b(?:\d[ -]*?){13,16}\b]])
MolochSession.register_body_feed("http", "cc_http_body")

