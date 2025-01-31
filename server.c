#include "message.h"

// Darwin
#include <bootstrap.h>
#include <mach/mach.h>
#include <mach/message.h>

// std
#include <stdio.h>
#include <stdlib.h>

mach_msg_return_t send_reply(mach_port_name_t port, const Message *inMessage) {
    Message response = {0};
    response.header.msgh_bits =
            inMessage->header.msgh_bits & // received message already contains
            MACH_MSGH_BITS_REMOTE_MASK;   // necessary remote bits to provide a
    // response, and it can differ
    // depending on SEND/SEND_ONCE right.

    response.header.msgh_remote_port = port;
    response.header.msgh_id = inMessage->header.msgh_id;
    response.header.msgh_size = sizeof(response);

    strcpy(response.bodyStr, "PONG!");
    response.count = inMessage->count + 1;

    return mach_msg(
            /* msg */ (mach_msg_header_t *)&response,
            /* option */ MACH_SEND_MSG,
            /* send size */ sizeof(response),
            /* recv size */ 0,
            /* recv_name */ MACH_PORT_NULL,
            /* timeout */ MACH_MSG_TIMEOUT_NONE,
            /* notify port */ MACH_PORT_NULL);
}

mach_msg_return_t
receive_msg(mach_port_name_t recvPort, ReceiveMessage *buffer) {
    mach_msg_return_t ret = mach_msg(
            /* msg */ (mach_msg_header_t *)buffer,
            /* option */ MACH_RCV_MSG,
            /* send size */ 0,
            /* recv size */ sizeof(*buffer),
            /* recv_name */ recvPort,
            /* timeout */ MACH_MSG_TIMEOUT_NONE,
            /* notify port */ MACH_PORT_NULL);
    if (ret != MACH_MSG_SUCCESS)
        return ret;

    Message *message = &buffer->message;

    printf("[SERVER] got message!\n");
    printf("  id      : %d\n", message->header.msgh_id);
    printf("  body    : %s\n", message->bodyStr);
    printf("  count   : %d\n", message->count);

    return MACH_MSG_SUCCESS;
}

int main() {
    mach_port_t task = mach_task_self();

    mach_port_name_t recvPort;
    if (mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &recvPort) != KERN_SUCCESS)
        return EXIT_FAILURE;

    if (mach_port_insert_right(task, recvPort, recvPort, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS)
        return EXIT_FAILURE;

    mach_port_t bootstrapPort;
    if (task_get_special_port(task, TASK_BOOTSTRAP_PORT, &bootstrapPort) != KERN_SUCCESS)
        return EXIT_FAILURE;

    if (bootstrap_register(bootstrapPort, "no.promon.mach-messages", recvPort) != KERN_SUCCESS)
        return EXIT_FAILURE;

    while (true) {
        // Message buffer.
        ReceiveMessage receiveMessage = {0};

        mach_msg_return_t ret = receive_msg(recvPort, &receiveMessage);
        if (ret != MACH_MSG_SUCCESS) {
            printf("[SERVER] Failed to receive a message: %#x\n", ret);
            continue;
        }

        // Continue if there's no reply port.
        if (receiveMessage.message.header.msgh_remote_port == MACH_PORT_NULL)
            continue;

        // Send a response.
        ret = send_reply(
                receiveMessage.message.header.msgh_remote_port,
                &receiveMessage.message);

        if (ret != MACH_MSG_SUCCESS)
            printf("[SERVER] Failed to respond: %#x\n", ret);
    }

    return 0;
}
