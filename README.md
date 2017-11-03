I have a VPN client that by default tunnels all traffic through it by creating a default route with a metric of 1 in the TCP routing table. I wanted to have split tunneling instead where only traffic to VPN addresses goes through the VPN adapter.

This program will monitor the windows TCP routing table. When it detects a change, it will look for a default route pointing to the adapter that you specifiy in the config.ini file next to VPNDesc= (you can find the name in device manager under network adapters). If it finds one, it will delete this default route and create routes for the ip addresses and masks that you specify in the config.ini file after addr1/mask1, addr2/mask2, etc... (you can add up to 100). When the program is closed it will delete the routes that it added and restore the default route that it deleted originally.

If you make changes to the config.ini file, you must restart the program in order for them to be applied.

This program must be run as administrator.

This was compiled using Dev-C++ 5.11 and TDM-GCC 4.9.2.
