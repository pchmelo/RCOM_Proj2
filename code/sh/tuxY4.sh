#!bin/bash

#part 1

ifconfig eth1 172.16.Y0.254/24
ifconfig eth2 172.16.Y1.253/24
sysctl net.ipv4.ip_forward=1
sysctl net.ipv4.icmp_echo_ignore_broadcasts=0
route add -net 172.16.1.0/24 gw 172.16.Y1.254
route -n

cat /etc/resolv.conf # Tem que ter o nameserver 10.227.20.3