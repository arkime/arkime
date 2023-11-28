sip = require("siphash")

function http_mb(session, data, dir, meta, glo)
  session:table().headers = {}
end

function http_header_field(session, data, dir)
  table.insert(session:table().headers, tostring(data))
end

function http_hc(session, data, dir)
  if dir == 0 then
    local hash = sip.siphash(table.concat(session:table().headers, ':'))
    -- Adds an attribute to the session which contains the hash of the headers
    session:add_string("http_req_hdr_hash", string.format("%016x", string.unpack("<I8", hash)))
  end
end

ArkimeSession.http_on(ArkimeSession.HTTP.HEADER_FIELD_RAW, "http_header_field")
ArkimeSession.http_on(ArkimeSession.HTTP.MESSAGE_BEGIN, "http_mb")
ArkimeSession.http_on(ArkimeSession.HTTP.HEADERS_COMPLETE, "http_hc")
