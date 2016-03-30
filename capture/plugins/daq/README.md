The daq reader plugin now works directly with daq


To use:
* install the daq package on the build hosts and all hosts that will run moloch-capture https://www.snort.org/downloads
* build the plugin by using ```make``` in the ```capture/plugins/daq``` directory
* load the daq plugin by changing configuration file so it has reader-daq.so as a rootPlugins ```rootPlugins=reader-daq.so```
* tell moloch-capture to use daq as the reader method with ```pcapReadMethod=daq`` in your configuration file
* optinally change ```interface``` to any special daq interface value

