#!bin/bash

ifconfig eth1 172.16.Y0.1/24
route add -net 172.16.Y1.0/24 gw 172.16.Y0.254
route add -net 172.16.1.0/24 gw 172.16.Y0.254
route -n

cat /etc/resolv.conf

ping google.com