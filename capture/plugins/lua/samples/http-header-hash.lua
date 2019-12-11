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

MolochSession.http_on(MolochSession.HTTP.HEADER_FIELD_RAW, "http_header_field")
MolochSession.http_on(MolochSession.HTTP.MESSAGE_BEGIN, "http_mb")
MolochSession.http_on(MolochSession.HTTP.HEADERS_COMPLETE, "http_hc")
