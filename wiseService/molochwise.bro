@load base/frameworks/files
@load base/frameworks/notice
@load frameworks/files/hash-all-files

module MolochWise;


export {
    redef enum Notice::Type += {
        Match
    };

    ## File types to attempt matching
    const match_file_types = /application\/x-dosexec/ |
                             /application\/vnd.ms-cab-compressed/ |
                             /application\/pdf/ |
                             /application\/x-shockwave-flash/ |
                             /application\/x-java-applet/ |
                             /application\/jar/ |
                             /video\/mp4/ &redef;


    # Set in local.bro with: redef MolochWise:base_url = "http://wise:80801";
    const base_url = "" &redef;
    const lookup_timeout: interval = 30min &redef;
    const lookup_count = 20 &redef;
}


global wise_matched: table[string] of string_vec &create_expire=lookup_timeout;
global wise_lookingup: set[string] &create_expire=1min;
global wise_next_lookups: table[string] of set[string] = table();

function wiseAvailable(key: string): bool
{
    return when (key in wise_matched) {
        return T;
    }
    timeout 30 sec {
        return F;
    }
}


function process_ip(key: string)
{
    local matches = wise_matched[key];
    if (matches[0] == "BRONONE") {
        return;
    }

    for (match_index in matches) {
	local parts = split_string1(matches[match_index], /\tBROSUB\t/);
	local n: Notice::Info = Notice::Info($note=Match, 
					     $msg=fmt("WISE hit on %s by %s", key, parts[0]), 
					     $sub=parts[1]);
	NOTICE(n);
    }
}

function process_md5(key: string, fi: Notice::FileInfo)
{
    local matches = wise_matched[key];
    if (matches[0] == "BRONONE") {
        return;
    }

    for (match_index in matches) {
	local parts = split_string1(matches[match_index], /\tBROSUB\t/);
	local n: Notice::Info = Notice::Info($note=Match, 
					     $msg=fmt("WISE hit on %s by %s", key, parts[0]), 
					     $sub=parts[1]);
        Notice::populate_file_info2(fi, n);
	NOTICE(n);
    }
}

function requestWise()
{
    print "MOLOCH", |wise_matched|, |wise_lookingup|, |wise_next_lookups["ip"]|, |wise_next_lookups["md5"]|;
    for (wtype in wise_next_lookups) {
	if (|wise_next_lookups[wtype]| == 0) {
	    next;
	}
	local data = "";
	for (key in wise_next_lookups[wtype]) {
	    delete(wise_next_lookups[wtype][key]);
	    add(wise_lookingup[key]);
	    if (data == "") {
		data += key;
	    } else {
		data += "," + key;
	    }
	}

	local url = fmt("%s/bro/%s?items=%s", base_url, wtype, data);
	local req: ActiveHTTP::Request = ActiveHTTP::Request($url=url, $method="GET");
	when (local res = ActiveHTTP::request(req)) {
	    if (|res| > 0) {
		if (res?$body) {
		    local lines = split_string(res$body, /\tBRONEXT\t/);
		    for (line_index in lines) {
			local parts = split_string1(lines[line_index], /\tBROIS\t/);
			wise_matched[parts[0]] = split_string(parts[1], /\tBROMORE\t/);
			delete(wise_lookingup[parts[0]]);
		    }
		}
	    }
	    return;
	}
    }
}


function wise_lookup_md5(key: string, fi: Notice::FileInfo)
{
    if (key in wise_matched) {
        process_md5(key, fi);
    } else if (key in wise_lookingup || key in wise_next_lookups["md5"]) {
        when (wiseAvailable(key)) {
            process_md5(key, fi);
        }
        timeout 30 sec {
        }
    } else {
        add(wise_next_lookups["md5"][key]);
        if (|wise_next_lookups["md5"]| > lookup_count) {
            requestWise();
        }
        when (wiseAvailable(key)) {
            process_md5(key, fi);
        }
        timeout 30 sec {
        }
    }
}

function wise_lookup_ip(ip: addr)
{
    
    local key = fmt("%s", ip);
    if (key in wise_matched) {
        process_ip(key);
    } else if (key in wise_lookingup || key in wise_next_lookups["ip"]) {
        when (wiseAvailable(key)) {
            process_ip(key);
        }
        timeout 30 sec {
        }
    } else {
        add(wise_next_lookups["ip"][key]);
        if (|wise_next_lookups["ip"]| > lookup_count) {
            requestWise();
        }
        when (wiseAvailable(key)) {
            process_ip(key);
        }
        timeout 30 sec {
        }
    }
}

event connection_established(c: connection)
{
  if ( c$orig$state == TCP_ESTABLISHED &&
       c$resp$state == TCP_ESTABLISHED ) {
    wise_lookup_ip(c$id$orig_h);
    wise_lookup_ip(c$id$resp_h);
  }
}

event file_hash(f: fa_file, kind: string, hash: string)
{
    if ( kind == "md5" && f$info?$mime_type && match_file_types in f$info$mime_type) {
        wise_lookup_md5(hash, Notice::create_file_info(f));
    }
    flush_all();
}

# Make sure nothing is waiting more then 5 seconds
event wisetimer()
{
    if (|wise_next_lookups["md5"]| > 0 || |wise_next_lookups["ip"]| > 0) {
        requestWise();
    }
    schedule 5sec {wisetimer()};
}

event bro_init()
{
    wise_next_lookups["md5"] = set();
    wise_next_lookups["ip"] = set();
    schedule 5sec {wisetimer()};
}
