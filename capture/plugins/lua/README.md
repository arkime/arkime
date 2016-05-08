Simple lua integration, currently experimental.

To use:
* install the lua package for your OS
* build the plugin by using ```make``` in the ```capture/plugins/lua``` directory
* load the lua plugin by change configuration file so it has lua.so as a plugins ```plugins=lua.so```
* set ```luaFiles``` to a list of lua files to load


How it works:
Each packet thread gets its own lua interpreter.
All interpreters load the same lua files.

Callbacks:

classifyFunction(session, data, which)

parserFunction(session, data, which)
* returns -1 to stop parsing


Commands:

moloch_parsers_classifier_register_tcp(name, offset, match, classifyFunction)

moloch_parsers_classifier_register_udp(name, offset, match, classifyFunction)

moloch_parsers_register(session, function)

moloch_session_add_tag(session, tag)

moloch_field_add_string(session, fieldExpressionOrFieldId, string)

moloch_field_add_int(session, fieldexpressionOrFieldId, int)

moloch_field_by_exp(fieldExpression
* returns -1 if not found, or the fieldId

