"""Singleton ACP stack."""

import logging
import threading
from typing import Any

import serial

from ..defs import BoapAcpMalformedMessageError
from .messages import BoapAcpMsg, BoapAcpMsgId, BoapAcpNodeId


class BoapAcp:
    """ACP stack."""

    HEADER_SIZE = 4
    PAYLOAD_RECV_TO = 1

    def __init__(self, port: Any, baud: int) -> None:
        """Initialize the ACP stack."""
        self.log = logging.getLogger("boap-gui-logger")
        self.port = port
        self.baud = baud
        self.write_lock = threading.Lock()

        # Open the serial port
        self.log.info("Opening the serial port...")
        self.serial = serial.Serial(port=self.port, baudrate=self.baud)

    def msg_create(
        self, receiver: BoapAcpNodeId, msg_id: BoapAcpMsgId
    ) -> BoapAcpMsg:
        """Create an ACP message."""
        return BoapAcpMsg(
            msg_id=msg_id,
            sender=BoapAcpNodeId.BOAP_ACP_NODE_ID_PC,
            receiver=receiver,
        )

    def msg_send(self, msg: BoapAcpMsg) -> None:
        """Send an ACP message."""
        serial_message = bytearray(
            [
                msg.get_id(),
                msg.get_sender(),
                msg.get_receiver(),
                msg.get_payload_size(),
            ]
        )
        if msg.payload:
            serial_message += bytearray(msg.payload.serialize())
        # Call serial API
        with self.write_lock:
            self.serial.write(serial_message)

    def msg_receive(self) -> BoapAcpMsg:
        """Receive an ACP message."""
        # Block on read (no timeout)
        msg = None
        while not msg:
            # Read from the serial port
            header = self.serial.read(self.HEADER_SIZE)

            # Validate the header
            if not self.__valid_header(header):
                # Flush the OS buffer...
                self.serial.read_all()
                # ...and try again
                continue

            # Parse the header
            msg_id, sender, receiver, payload_size = header

            payload = None
            if payload_size:
                # Received the payload if any declared in the header
                timed_out = False

                def timer_callback() -> None:
                    nonlocal timed_out
                    timed_out = True
                    self.serial.cancel_read()

                # Start a timer to not block waiting for payload indefinitely
                timer = threading.Timer(self.PAYLOAD_RECV_TO, timer_callback)
                timer.start()
                payload = self.serial.read(payload_size)
                timer.cancel()
                if timed_out:
                    self.log.warning(
                        "Failed to receive the payload in time"
                        + f" (header: id=0x{msg_id:02X},"
                        + f" sender=0x{sender:02X}, "
                        + f" receiver=0x{receiver:02X},"
                        + f" payload_size={payload_size})"
                    )
                    continue

            try:
                msg = BoapAcpMsg(
                    msg_id=msg_id,
                    sender=sender,
                    receiver=receiver,
                    serial_payload=payload,
                )
            except BoapAcpMalformedMessageError:
                self.log.error(
                    "Failed to parse the payload of message"
                    + f" 0x{msg_id:02X} from 0x{sender:02X}"
                )
                # Flush the OS buffer
                self.serial.read_all()
        return msg

    def __valid_header(self, header: bytes) -> bool:
        """Validate an ACP header."""
        msg_id, sender, receiver, _ = header
        return (
            receiver == BoapAcpNodeId.BOAP_ACP_NODE_ID_PC
            and sender in list(BoapAcpNodeId)
            and msg_id in list(BoapAcpMsgId)
        )
