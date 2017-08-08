
# collect all the sources
file(GLOB SRC_FILES 
	api/api_lib.c
	api/api_msg.c
	api/err.c
	api/netbuf.c
	api/netdb.c
	api/netifapi.c
	api/sockets.c
	api/tcpip.c
	netif/etharp.c
	core/def.c
	core/init.c
	core/mem.c
	core/pbuf.c
	core/stats.c
	core/tcp.c
	core/sys_arch.c
	core/timers.c
	core/dhcp.c
	core/ipv4/autoip.c
	core/ipv4/icmp.c
	core/ipv4/igmp.c
	core/ipv4/inet.c
	core/ipv4/inet_chksum.c
	core/ipv4/ip_addr.c
	core/ipv4/ip.c
	core/ipv4/ip_frag.c
	core/memp.c
	core/raw.c
	core/tcp_in.c
	core/udp.c
	core/dns.c
	core/netif.c
	core/sys.c
	core/tcp_out.c
)

# add shared library to system 
add_library(lwip STATIC ${SRC_FILES})
