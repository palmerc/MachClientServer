#include "message.h"

// Darwin
#include <bootstrap.h>
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/message.h>
#include <mach/port.h>
#include <mach/task.h>

// std
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MS_IN_S 1000

static int count = 0;


mach_msg_return_t
receive_msg(mach_port_name_t recvPort, mach_msg_timeout_t timeout) {
    // Message buffer.
    ReceiveMessage receiveMessage = {0};

    mach_msg_return_t ret = mach_msg(
            /* msg */ (mach_msg_header_t *)&receiveMessage,
            /* option */ MACH_RCV_MSG | MACH_RCV_TIMEOUT,
            /* send size */ 0,
            /* recv size */ sizeof(receiveMessage),
            /* recv_name */ recvPort,
            /* timeout */ timeout,
            /* notify port */ MACH_PORT_NULL);
    if (ret != MACH_MSG_SUCCESS) {
        return ret;
    }

    Message *message = &receiveMessage.message;

    printf("[CLIENT] got response message!\n");
    printf("  id      : %d\n", message->header.msgh_id);
    printf("  body    : %s\n", message->bodyStr);
    printf("  count   : %d\n", message->count);
    count = message->count;

    return MACH_MSG_SUCCESS;
}



int main() {
  // get the task
  mach_port_name_t task = mach_task_self();
  printf("[CLIENT] no.promon.mach-messages task 0x%x\n", task);

  // get the bootstrap port 
  mach_port_t bootstrapPort;
  if (task_get_special_port(task, TASK_BOOTSTRAP_PORT, &bootstrapPort) != KERN_SUCCESS)
    return EXIT_FAILURE;
  printf("[CLIENT] no.promon.mach-messages bootstrap port 0x%x\n", bootstrapPort);

  mach_port_t port;
  if (bootstrap_look_up(bootstrapPort, "no.promon.mach-messages", &port) != KERN_SUCCESS)
    return EXIT_FAILURE;
  printf("[CLIENT] no.promon.mach-messages remote port 0x%x\n", port);

  mach_port_t replyPort;
  if (mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &replyPort) != KERN_SUCCESS)
    return EXIT_FAILURE;

  if (mach_port_insert_right(task, replyPort, replyPort, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS)
    return EXIT_FAILURE;

  while (true) {
      Message message = {0};
      message.header.msgh_remote_port = port;
      message.header.msgh_local_port = replyPort;

      // copy send right on the remote port
      message.header.msgh_bits = MACH_MSGH_BITS_SET(
      /* remote */ MACH_MSG_TYPE_COPY_SEND,
      /* local */ MACH_MSG_TYPE_MAKE_SEND_ONCE,
      /* voucher */ 0,
      /* other */ 0);
      message.header.msgh_id = 4;
      message.header.msgh_size = sizeof(message);

      strcpy(message.bodyStr, "Ping!");
      message.count = count;

      mach_msg_return_t ret = mach_msg(
              /* msg */ (mach_msg_header_t *) &message,
              /* option */ MACH_SEND_MSG,
              /* send size */ sizeof(message),
              /* recv size */ 0,
              /* recv_name */ MACH_PORT_NULL,
              /* timeout */ MACH_MSG_TIMEOUT_NONE,
              /* notify port */ MACH_PORT_NULL);

      if (ret != MACH_MSG_SUCCESS) {
          printf("[CLIENT] Failed mach_msg: %d\n", ret);
          return EXIT_FAILURE;
      }

      while (ret == MACH_MSG_SUCCESS)
          ret = receive_msg(replyPort, /* timeout */ 1 * MS_IN_S);

      if (ret == MACH_RCV_TIMED_OUT)
          printf("[CLIENT] Receive timed out, no more messages from alice yet.\n");
      else if (ret != MACH_MSG_SUCCESS) {
          printf("[CLIENT] Failed to receive a message: %#x\n", ret);
          return 1;
      }
  }

  return 0;
}
