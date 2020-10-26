**entropy.lua** - Adds a field with the entropy of DNS and HTTP traffic

**dcerpc.lua** - Adds a field for OpCode used during DCERPC communication

**smb.lua** - Adds the OpCode and Command used in SMB

**Config.ini**

The below code is added to the bottom of the config.ini to enable the fields

```
[custom-views]
entropy=title:Entropy;require:entropy;fields:entropy.dns,entropy.http
dcerpc=title:DCERPC;require:dcerpc;fields:dcerpc.api,dcerpc.cmd

[custom-fields]
entropy.dns=db:entropy.dns;kind:integer;friendly:Entropy DNS;count:false;help:Entropy of DNS
entropy.http=db:entropy.http;kind:integer;friendly:Entropy HTTP;count:false;help:Entropy of the HTTP body
smb.opcode=db:smb.opcode;kind:termfield;friendly:SMB OpCode;count:true;help:Operation code
smb.cmd=db:smb.cmd;kind:termfield;friendly:SMB Command;count:true;help:Command
dcerpc.api=db:dcerpc.api;kind:termfield;friendly:DCERPC API;count:true;help:API used during DCERPC communication
dcerpc.cmd=db:dcerpc.cmd;kind:termfield;friendly:DCERPC Command;count:true;help:OpCode used during DCERPC communication
```
You also need to add lua.so to plugins and the script files as listed below

* plugins=lua.so
* luaFiles=/data/moloch/lua/dcerpc.lua;/data/moloch/lua/smb.lua;/data/moloch/lua/entropy.lua
