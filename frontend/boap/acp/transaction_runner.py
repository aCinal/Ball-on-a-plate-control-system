"""Transaction running utility."""

import queue

from ..defs import BoapAcpTransactionError
from . import get_acp_stack_handle
from .messages import BoapAcpMsg, BoapAcpMsgId, BoapAcpMsgPayload


class TransactionRunner:
    """Transaction running utility."""

    def __init__(self, msg_queue: queue.Queue, receive_timeout: float) -> None:
        """Instantiate a transaction runner."""
        self.msg_queue = msg_queue
        self.receive_timeout = receive_timeout

    def run_transaction(
        self, request: BoapAcpMsg, expected_response_id: BoapAcpMsgId
    ) -> BoapAcpMsgPayload:
        """Run an ACP transaction."""
        # Send the request message
        get_acp_stack_handle().msg_send(request)
        # Wait for the response
        try:
            response: BoapAcpMsg = self.msg_queue.get(
                timeout=self.receive_timeout
            )
            if expected_response_id != response.get_id():
                raise BoapAcpTransactionError(
                    "ACP transaction failed. Expected response"
                    + f" 0x{expected_response_id:02X},"
                    + f" instead received 0x{response.get_id():02X}"
                )
            return response.get_payload()
        except queue.Empty:
            raise BoapAcpTransactionError(
                "ACP transaction failed. No response received"
                + f" for message 0x{request.get_id():02X}"
                + f" (expected response: 0x{expected_response_id:02X})"
            )
