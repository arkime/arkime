#!/bin/sh

# Check presence of arguments
if [ $# -eq 0 ]; then
	echo "Usage: ./arkime_config_interfaces.sh -c <Arkime config.ini file> [-n <node>]"
	echo "Run $0 -h for help !"
	exit 0
fi

# Set default value. Will be overwrite with options.
node="default"
file="/opt/arkime/etc/config.ini"

# Check options
while getopts "hn:c:" option; do
	case $option in
		n)
			node=$OPTARG
			;;
		c)
			file=$OPTARG
			;;
		h)
			echo "Usage: ./arkime_config_interfaces.sh -c <Arkime config.ini file> [-n <node>]"
			echo "  -c  to specify the configuration file."
			echo "  -n  to specify the node."
			exit 0
			;;
	esac
done

# Check the existence of the file
if [ ! -f $file ]; then
	echo "File : "$file" seems to be absent/incorrect."
	exit 1
fi

# Extract list of interfaces
interfaces=$(sed -n "/^\[$node\]\s*$/,/^\[.*\]\s*$/ {p;}" $file | grep "interface" | awk -F "=" '{print $2}' | sed 's/;/ /g')

# Check interfaces, force to default if no interface.
if [ -z "$interfaces" ]; then
	interfaces=$(sed -n -e "/^\[default\]\s*$/,/^\[.*\]\s*$/ {p;}" $file | grep "interface" | awk -F "=" '{print $2}' | sed 's/;/ /g')
fi

# Apply settings
for interface in $interfaces ; do
	# Verif NIC existence.
	checkNic=$(ip a | grep $interface)
	if [ ! -z "$checkNic" ]; then
		/usr/sbin/ip link set $interface up || true
		/usr/sbin/ip link set $interface promisc on || true
		/sbin/ethtool -G $interface rx 4096 tx 4096 || true
  		for i in rx tx sg tso ufo gso gro lro; do
      			/sbin/ethtool -K $interface $i off || true
  		done
	else
		echo "\nATTENTION :\nNIC : "$interface" seems to be absent/incorrect."
		echo "Please check your configuration file : "$file"\n"
	fi
done
