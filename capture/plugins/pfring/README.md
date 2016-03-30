The pfring reader plugin now works directly with the pfring rpm, skipping the pfring pcap userland bridge.


To use:
* install the pfring package on the build hosts and all hosts that will run moloch-capture
  http://packages.ntop.org/
* build the plugin by using ```make``` in the ```capture/plugins/pfring``` directory
* load the pfring plugin by changing configuration file so it has reader-pfring.so as a rootPlugins ```rootPlugins=reader-pfring.so```
* tell moloch-capture to use pfring as the reader method with ```pcapReadMethod=pfring`` in your configuration file
* optionaly set ```pfringClusterId``` in configuration file 
* optinally change ```interface``` to any special pfring interface value

