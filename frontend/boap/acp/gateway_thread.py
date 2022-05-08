"""ACP gateway thread."""

import logging
import threading
from queue import Queue
from typing import List

from . import get_acp_stack_handle
from .messages import BoapAcpMsg, BoapAcpMsgId, SBoapAcpLogCommit


class LocalRouting:
    """Message routing table entry."""

    def __init__(self, queue: Queue, msg_ids: List[BoapAcpMsgId]) -> None:
        """Initialize a routing table entry."""
        self.queue = queue
        self.msg_ids = msg_ids


class Gateway:
    """ACP gateway thread."""

    def __init__(self, routing_table: List[LocalRouting]) -> None:
        """Initialize and start an ACP gateway."""
        self.log = logging.getLogger("boap-gui-logger")
        self.remote_log = logging.getLogger("boap-remote-logger")
        self.routing_table = routing_table

        self.log.debug("Starting the gateway thread...")
        # Create the gateway thread
        self.gateway_thread = threading.Thread(
            target=self.__gateway_thread_entry_point,
            name="GatewayThread",
            daemon=True,
        )
        self.gateway_thread.start()

    def __gateway_thread_entry_point(self) -> None:
        """ACP gateway thread entry point."""
        self.log.debug("Gateway thread entered")

        while True:
            # Block on receive
            message = get_acp_stack_handle().msg_receive()

            if message:
                if BoapAcpMsgId.BOAP_ACP_PING_REQ == message.get_id():
                    # Respond to ping requests
                    pingResponse = get_acp_stack_handle().msg_create(
                        message.get_sender(), BoapAcpMsgId.BOAP_ACP_PING_RESP
                    )
                    get_acp_stack_handle().msg_send(pingResponse)
                elif BoapAcpMsgId.BOAP_ACP_LOG_COMMIT == message.get_id():
                    payload = message.get_payload()
                    assert isinstance(payload, SBoapAcpLogCommit)
                    # Print log messages locally
                    self.remote_log.info(
                        f"[0x{message.get_sender():02X}] " + payload.Message
                    )
                else:
                    # Forward remaining traffic to appropriate queues
                    self.__handle_application_message(message)

    def __handle_application_message(self, message: BoapAcpMsg) -> None:
        """Handle application message."""
        for route in self.routing_table:
            if message.get_id() in route.msg_ids:
                route.queue.put(message)
                return
        self.log.warning(
            f"Message ID 0x{message.get_id():02X} not found in the"
            + f" routing table (sender: 0x{message.get_sender():02X})"
        )
