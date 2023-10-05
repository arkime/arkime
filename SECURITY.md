# Security Policy

The Arkime project takes security very seriously, but any complex software project is going to have some vulnerabilities. 
Please submit any security issues to [HackerOne](https://hackerone.com/yahoo) or arkime.security@yahooinc.com, please use [github issues](https://github.com/arkime/arkime/issues) or slack for non security issues.

## Scope
Examples of security items in scope

* all - Stored XSS
* all - Buffer overflow
* all - UI/API permission checking
* viewer - Bypassing forced expressions (excluding when a sessionId is known)

## Out of Scope
Examples of security items that are out of scope and maybe should be submitted to our github issues page

* all - Crash or security issue on startup from bad config settings or command line options
* all - Admins configuring bad/dangerous URLs/items in notifiers, clusters, WISE sources, etc
* all - Auth brute force, http-digest weaknesses, lack of rate limiting, md5 usage
* all - Most issues around anonymous auth mode
* all - Most OSC52 issues
* viewer - Directly accessing a session or session pcap using the sessionId
* viewer - Viewing the results of another user’s hunt
* wise - /dump end point

## Known issues

Security Issues that are known and are either not fixable or a known limitation of Arkime.

* IP TTL Expiry Attacks - An attacker can manipulate IP TTLs so that Arkime will see packets that the end host will not see. The only “fix” is to add TTL normalization at the network border. Future versions of Arkime may try and detect this attack.
* Not all tunnel protocols are supported by Arkime
