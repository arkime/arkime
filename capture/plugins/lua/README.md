Simple lua integration, currently experimental.  It supports writing simple protocol classifiers and parsers and http requests.

To use:
* install the lua package for your OS, requires at least 5.3
* build the plugin by using ```make``` in the ```capture/plugins/lua``` directory
* load the lua plugin by change configuration file so it has lua.so as a plugins ```plugins=lua.so```
* set ```luaFiles``` to a list of lua files to load


How it works:
Each packet thread gets its own lua interpreter.
All interpreters load the same lua files.

## Callbacks:

### classifyFunction(session, data, direction)
Callback when the initial part of the data stream matches the details set by either moloch_parsers_classifier_register_tcp or moloch_parsers_classifier_register_udp.  It may be called multiple times for the same session if the first packets in each direction matches.  It is only called with the first packet of data, you want to see more call moloch_parseres_register
* session = opaque userdata to moloch session
* data = the binary data, will always be from start of session
* direction = traffic direction

### parserFunction(session, data, direction)
Callback that receives the stream of data for session.  Will be called multiple times, basically for each packet received although for TCP sometimes packets are combined before calling.
* session = opaque userdata to moloch session
* data = the next chunk of binary data
* direction = traffic direction
* returns = -1 to stop parsing
 
### httpResponseFunction(code, data)
Callback to moloch_http_request.  It received the full data response.
* code = response code
* data = the response data

## Fields Commands
### moloch_field_by_exp(fieldExpression)
Look up a field expression and return the fieldId
* fieldExpression = the expression used in search box
* returns = the fieldId

### moloch_field_add_string(session, fieldExpressionOrFieldId, value)
Add a string value to a session
* session = opaque userdata to moloch session
* fieldExpressionOrFieldId = the field expression or a fieldId
* value = the string to add

### moloch_field_add_int(session, fieldexpressionOrFieldId, value)
Add a integer value to a session
* session = opaque userdata to moloch session
* fieldExpressionOrFieldId = the field expression or a fieldId
* value = the string to add

## Session Commands
### moloch_session_add_tag(session, tag)
Add a tag to a session
* session = opaque userdata to moloch session
* tag = the string to add to the tags field

### moloch_session_incr_outstanding(session)
Tell moloch that there is an async operation happening to the session, such as waiting for a http response
* session = opaque userdata to moloch session

### moloch_session_decr_outstanding(session)
Tell moloch that an async operation for the session finished
* session = opaque userdata to moloch session

## Parsers Commands

### moloch_parsers_classifier_register_tcp(name, offset, match, classifyFunctionName)
Add a TCP classifier to match initial session packets against.
* name = The human name of the classifier
* offset = Where in the initial packet the match will be made
* match = Binary data to match
* classifyFunctionName = the string name of the lua function to call.  Function should implement the classifyFunction signature above

### moloch_parsers_classifier_register_udp(name, offset, match, classifyFunctionName)
Add a UDP classifier to match initial session packets against.
* name = The human name of the classifier
* offset = Where in the initial packet the match will be made
* match = Binary data to match
* classifyFunctionName = the string name of the lua function to call.  Function should implement the classifyFunction signature above.

### moloch_parsers_register(session, function)
Inside a classify callback this function registers that the entire stream should be parsed.
* session = opaque userdata to moloch session
* function = the lua function to call with all the data.  Function should implement the parserFunction signature above.


## HTTP Commands

### moloch_http_create_server(hostports, maxConnections, maxRequests)
It is assumed that the same server will be contacted repeatedly, so for each unique host you first create a server object.
* hostports = comma seperated list of base urls
* maxConnections = Maximum number of connections allowed
* maxRequests = Maximum number of outstanding requests
* returns = the userdata server object for other calls

### moloch_http_request(server, method, path, data, function)
This actually makes a request to the server created with moloch_http_create_server
* server = the userdata server object
* method = the method string
* path = the full path and query string, already encoded
* data = Use "" for GET, otherwise the data to send
* function = the lua function to call with the results.  Function should implement the httpResponseFunction signature above.

### moloch_http_free_server
Free the server userdata object
* server = the userdata server object
