"""Keepalive thread."""

import logging
import queue
import threading
import time

from . import get_acp_stack_handle
from .messages import BoapAcpMsg, BoapAcpMsgId, BoapAcpNodeId


class Keepalive:
    """Keepalive thread."""

    PING_PERIOD = 10
    RESPONSE_TIMEOUT = 1

    def __init__(self) -> None:
        """Initialize and start the keepalive thread."""
        self.log = logging.getLogger("boap-gui-logger")

        # Create the message queue
        self.msg_queue: queue.Queue = queue.Queue()

        self.log.debug("Starting the keepalive thread...")
        # Create the keepalive thread
        self.keepalive_thread = threading.Thread(
            target=self.__keepalive_thread_entry_point,
            name="KeepaliveThread",
            daemon=True,
        )
        self.keepalive_thread.start()

    def get_message_queue(self) -> queue.Queue:
        """Get keepalive thread's message queue."""
        return self.msg_queue

    def __keepalive_thread_entry_point(self) -> None:
        """Thread entry point."""
        self.log.debug("Keepalive thread entered")

        while True:
            time.sleep(self.PING_PERIOD)

            # Ping the plant
            self.__ping_and_wait_for_response(
                BoapAcpNodeId.BOAP_ACP_NODE_ID_PLANT
            )
            # Ping the controller
            self.__ping_and_wait_for_response(
                BoapAcpNodeId.BOAP_ACP_NODE_ID_CONTROLLER
            )

    def __ping_and_wait_for_response(self, node: BoapAcpNodeId) -> None:
        """Ping a BOAP node and wait for response."""
        request = get_acp_stack_handle().msg_create(
            node, BoapAcpMsgId.BOAP_ACP_PING_REQ
        )
        get_acp_stack_handle().msg_send(request)
        try:
            response: BoapAcpMsg = self.msg_queue.get(
                timeout=self.RESPONSE_TIMEOUT
            )
            # Assert message from the correct receiver
            if response.get_sender() != node:
                self.log.warning(
                    f"Sent ping request to 0x{node:02X}."
                    + f"0x{response.get_sender():02X} responded instead"
                )
        except queue.Empty:
            self.log.warning(f"Node 0x{node:02X} failed to respond to ping")
