The snf reader plugin now works directly with Myricom SNF
  
Optional settings
* ```snfNumRings``` number of rings and threads to use per interface
* ```snfDataRingSize``` SNF_DATARING_SIZE
* ```snfFlags``` Variable that controls process-sharing (1), port aggregation (2), and packet duplication (3), see SNF documentation for more details.
* ```snfNumProcs``` The number of capture processes sharing a given Myricom board
* ```snfProcNum``` The process number in the sharing cluster (If snfNumProcs=2, one process should have snfProcNum=1, the other =2)

Multiple capture processes per NIC requires some special settings
* Set the ```SNF_APP_ID``` environment variable to a unique (per host) value
* Make sure ```snfFlags``` includes the process sharing flag (0x01)
* Set ```snfNumProcs``` to the numer of capture processes listening on the shared interface
* Set the ```snfProcNum``` for each process


To use:
* install the snf package on the build hosts and all hosts that will run moloch-capture
* build the plugin by using ```make``` in the ```capture/plugins/snf``` directory
* load the snf plugin by changing configuration file so it has reader-snf.so as a rootPlugins ```rootPlugins=reader-snf.so```
* tell moloch-capture to use snf as the reader method with ```pcapReadMethod=snf`` in your configuration file
* change ```interface``` to either the OS interface name or snf# where # is the portnum
