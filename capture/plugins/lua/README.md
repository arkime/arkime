Simple lua integration, currently experimental.  It supports writing simple protocol classifiers and parsers and http requests.

To use:
* install the lua package for your OS, requires at least 5.3
* build the plugin by using ```make``` in the ```capture/plugins/lua``` directory
* load the lua plugin by change configuration file so it has lua.so as a plugins ```plugins=lua.so```
* set ```luaFiles``` to a list of lua files to load


How it works:
Each packet thread gets its own lua interpreter.
All interpreters load the same lua files configured by ```luaFiles```

## Callbacks:

### classifyFunction(session, data, direction)
Callback when the initial part of the data stream matches the details set by either moloch_parsers_classifier_register_tcp or moloch_parsers_classifier_register_udp.  It may be called multiple times for the same session if the first packets in each direction matches.  It is only called with the first packet of data, you want to see more call moloch_parseres_register
* session = MolochSession object
* data = the binary data, will always be from start of session
* direction = traffic direction

### parserFunction(session, data, direction)
Callback that receives the stream of data for session.  Will be called multiple times, basically for each packet received although for TCP sometimes packets are combined before calling.
* session = MolochSession object
* data = the next chunk of binary data
* direction = traffic direction
* returns = -1 to stop parsing
 
### httpResponseFunction(code, data)
Callback to moloch_http_request.  It received the full data response.
* code = response code
* data = the response data

## Moloch
Moloch.expression_to_fieldId(fieldExpression)
Look up a field expression and return the fieldId
* fieldExpression = the expression used in search box
* returns = the fieldId

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



### session:add_string(fieldExpressionOrFieldId, value)
Add a string value to a session
* fieldExpressionOrFieldId = the field expression or a fieldId
* value = the string to add
* returns = true if added, false if already there

### session:add_int(fieldexpressionOrFieldId, value)
Add a integer value to a session
* fieldExpressionOrFieldId = the field expression or a fieldId
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
