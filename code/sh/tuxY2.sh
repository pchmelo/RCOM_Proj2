#!bin/bash

#part 1


ifconfig eth1 172.16.Y1.1/24
route add -net 172.16.Y0.0/24 gw 172.16.Y1.253
route add -net 172.16.1.0/24 gw 172.16.Y1.254
route -n

cat /etc/resolv.conf # Tem que ter o nameserver 10.227.20.3

ping google.com
