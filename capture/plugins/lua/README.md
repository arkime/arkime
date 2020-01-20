Simple lua integration, currently experimental.  It supports (with more coming all the time)
* writing simple protocol classifiers 
* writing simple protocol parsers
* performing async http requests
* parsing http bodies

To use:
* install the lua package for your OS, requires at least 5.3
* build the plugin by using ```make``` in the ```capture/plugins/lua``` directory
* load the lua plugin by change configuration file so it has lua.so as a plugins ```plugins=lua.so```
* set ```luaFiles``` to a list of lua files to load


How it works:
* Each packet thread gets its own lua interpreter.
* Packets/Sessions are consistantly load balanced, so a 5 tuple will hit the same thread/lua interpreter
* All interpreters load the same lua files configured by ```luaFiles```

## Callbacks:

### classifyFunction(session, str, direction)
Callback when the initial part of the data stream matches the details set by either moloch_parsers_classifier_register_tcp or moloch_parsers_classifier_register_udp.  It may be called multiple times for the same session if the first packets in each direction matches.  It is only called with the first packet of data, you want to see more call moloch_parseres_register
* session = A MolochSession object
* str = A lua string with the binary data from start of session
* direction = traffic direction

### parserFunction(session, str, direction)
Callback that receives the stream of data for session.  Will be called multiple times, basically for each packet received although for TCP sometimes packets are combined before calling.
* session = A MolochSession object
* str = A lua string with the next chunk of binary data
* direction = traffic direction
* returns = -1 to stop parsing
 
### httpResponseFunction(code, str)
Callback to moloch_http_request.  It received the full data response.
* code = response code
* str = A lua string with full response

### bodyFeedFunction(session, data)
Generic body feed function
* session = A MolochSession object
* data = A MolochData object with the next chunk of binary data

### httpCallbackFunction(session, data, direction)
Generic http callback function
* session = A MolochSession object
* data = A MolochData object with the next chunk of data (or nil if not applicable)
* direction = 0 if this is a Request, 1 if this is a Response

### saveCallbackFunction(session, final)
Generic save callback function
* session = A MolochSession object
* final = boolean if this is the final save or not

## Moloch
Moloch.expression_to_fieldId(fieldExpression)
Look up a field expression and return the fieldId
* fieldExpression = the expression used in search box
* returns = the fieldId

## MolochData
A MolochData object is a wrapper for a C string that has access to pcre and other commands.  The main purpose is so we don't have to copy strings back and forth from lua and C.  The object can NOT be saved in a table or used in a closure directly, however a :copy version can be. It will throw an error if this rule is violated.
### MolochData.pcre_create(str)
Create a PCRE pattern to use for matching
* str = the expression
* returns = a user data object of compiled pcre expression

### MolochData.pattern_create(str)
Create a glob pattern to use for matching
* str = the expression
* returns = a user data object of compiled pattern

### MolochData.new(str)
* str = the lua string to convert into a new MolochData
* returns = the new MolochData object

### data:memmem(str)
Is str inside of data
* str = the lua string to check for
* returns = true if present

### data:pattern_ismatch(compiledPattern)
Perform a glob match
* compiledPattern = results of a previous MolochData.pattern_create call
* returns = true if match

### data:pcre_ismatch(compiledPCRE)
Perform a pcre match
* compiledPCRE = results of a previous MolochData.pcre_create call
* returns = true if match

### data:pcre_match(compiledPCRE)
Perform a pcre match with results
* compiledPCRE = results of a previous MolochData.pcre_create call
* returns = true if match, the match, any groupings

### data:get()
Return the lua string version
* returns = the lua string version

### data:copy()
Make a copy of a MolochData for later use, such as in a table or in a closure
* returns = a copy of the MolochData


## MolochSession
### MolochSession.register_tcp_classifier(name, offset, match, classifyFunctionName)
Add a TCP classifier to match initial session packets against.
* name = The human name of the classifier
* offset = Where in the initial packet the match will be made
* match = Binary data to match
* classifyFunctionName = the string name of the lua function to call.  Function should implement the classifyFunction signature above

### MolochSession.register_udp_classifier(name, offset, match, classifyFunctionName)
Add a UDP classifier to match initial session packets against.
* name = The human name of the classifier
* offset = Where in the initial packet the match will be made
* match = Binary data to match
* classifyFunctionName = the string name of the lua function to call.  Function should implement the classifyFunction signature above.

### MolochSession.register_body_feed(type, bodyFeedFunctionName)
Register to receive a feed of chunks of data from payload bodies
* type = The type of body feed to receive ("http", "smtp")
* bodyFeedFunctionName = the string name of the lua function to call.  Function should implement the bodyFeedFunction signature above.

### MolochSession.http_on(type, httpCallbackFunctionName)
Register to receive a feed of data from the http parser. There are eight different types of callback.
* type = the type of callback to register. This should be one of the constants:
  - MolochSession.HTTP.MESSAGE_BEGIN
  - MolochSession.HTTP.URL
  - MolochSession.HTTP.HEADER_FIELD
  - MolochSession.HTTP.HEADER_FIELD_RAW -- this is just like HEADER_FIELD except that the string is not lower-cased.
  - MolochSession.HTTP.HEADER_VALUE
  - MolochSession.HTTP.HEADERS_COMPLETE
  - MolochSession.HTTP.BODY
  - MolochSession.HTTP.MESSAGE_COMPLETE
* httpCallbackFunctionName = the string name of the lua function to call.  Function should implement the httpCallbackFunction signature above. If there is no data (HEADERS_COMPLETE, MESSAGE_COMPLETE) then the data argument will be nil and can be ignored. The method string is passed into the MESSAGE_BEGIN callback on the request side.

### MolochSession.register_pre_save(preSaveFunctionName)
Register to receive a callback before saving.  This function can call the incr_outstanding on the session to pause the save.
* preSaveFunctionName = the string name of the lua function to call.  Function should implement the saveCallbackFunction signature above.

### MolochSession.register_save(saveFunctionName)
Register to receive a callback as saving.  This function can NOT call the incr_outstanding on the session to pause the save.
* saveFunctionName = the string name of the lua function to call.  Function should implement the saveCallbackFunction signature above.


### session:add_string(fieldExpressionOrFieldId, value)
Add a string value to a session
* fieldExpressionOrFieldId = the field expression or a fieldId.
* value = the string to add
* returns = true if added, false if already there

### session:add_int(fieldexpressionOrFieldId, value)
Add a integer value to a session
* fieldExpressionOrFieldId = the field expression or a fieldId.
* value = the string to add
* returns = true if added, false if already there

### session:add_tag(tag)
Add a tag to a session
* tag = the string to add to the tags field

### session:add_protocol(protocol)
Short cut to session:add_string("protocols", protocol)
* session = MolochSession object
* protocol = the protocol string

### session:has_protocol(protocol)
Check to see if protocol has already been added session
* protocol = the protocol string
* returns = true if already present

### session:incr_outstanding()
Tell moloch that there is an async operation happening to the session, such as waiting for a http response.

### session:decr_outstanding()
Tell moloch that an async operation for the session finished

### session:register_parser(function)
Used usually inside a classify callback this function registers that the entire stream should be parsed.
* function = the lua function to call with all the data.  Function should implement the parserFunction signature above.

### session:table()
Return a table that can be used to set/get lua variables to share state across all callbacks for a session
* returns = a lua table

### session.ipProtocol
Returns a string containing the protocol.

### session.srcIp
Returns a string containing the source IP address.

### session.dstIp
Returns a string containing the destination IP address.

### session.srcPort
Returns the source port as a number.

### session.dstPort
Returns the destination port as a number.


## MolochHttpService

### MolochHttpService.new(hostports, maxConnections, maxRequests)
It is assumed that the same server will be contacted repeatedly, so for each unique host you first create a MolochHttpService object.
* hostports = comma seperated list of base urls
* maxConnections = Maximum number of connections allowed
* maxRequests = Maximum number of outstanding requests
* returns = the userdata server object for other calls

### service:request(method, path, data, function)
This actually makes a request to the server created with MolochHttpService.new()
* server = the userdata server object
* method = the method string
* path = the full path and query string, already encoded
* data = Use "" for GET, otherwise the data to send
* function = the lua function to call with the results.  Function should implement the httpResponseFunction signature above.
