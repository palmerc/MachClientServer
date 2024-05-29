cmake_minimum_required( VERSION 3.24 )
project( mach-messages )

add_executable( client client.c
        message.h)
add_executable( server server.c
        message.h)
