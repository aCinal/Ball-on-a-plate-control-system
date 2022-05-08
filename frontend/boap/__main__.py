"""Application entry point."""

import argparse
import sys

from .acp import initialize_acp_stack
from .acp.gateway_thread import Gateway, LocalRouting
from .acp.keepalive_thread import Keepalive
from .acp.messages import BoapAcpMsgId
from .gui import BoapGui


def main() -> int:
    """Application entry point."""
    # Parse command line arguments
    opts = parse_args()

    # Initialize the GUI
    gui = BoapGui(opts.debug)

    # Initialize the ACP stack
    initialize_acp_stack(opts.port, opts.baud)

    # Start the keepalive thread
    keepalive = Keepalive()

    # Start the gateway thread
    Gateway(
        [
            LocalRouting(
                keepalive.get_message_queue(),
                [BoapAcpMsgId.BOAP_ACP_PING_RESP],
            ),
            LocalRouting(
                gui.get_trace_panel_message_queue(),
                [BoapAcpMsgId.BOAP_ACP_BALL_TRACE_IND],
            ),
            LocalRouting(
                gui.get_control_panel_configurator_message_queue(),
                [
                    BoapAcpMsgId.BOAP_ACP_GET_PID_SETTINGS_RESP,
                    BoapAcpMsgId.BOAP_ACP_SET_PID_SETTINGS_RESP,
                    BoapAcpMsgId.BOAP_ACP_GET_SAMPLING_PERIOD_RESP,
                    BoapAcpMsgId.BOAP_ACP_SET_SAMPLING_PERIOD_RESP,
                    BoapAcpMsgId.BOAP_ACP_GET_FILTER_ORDER_RESP,
                    BoapAcpMsgId.BOAP_ACP_SET_FILTER_ORDER_RESP,
                ],
            ),
            LocalRouting(
                gui.get_control_panel_trace_enable_message_queue(),
                [BoapAcpMsgId.BOAP_ACP_BALL_TRACE_ENABLE],
            ),
        ]
    )

    # Run the PyQt event loop
    return gui.run()


def parse_args() -> argparse.Namespace:
    """Parse command line arguments."""
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "-p",
        "--port",
        help="Port at which the router is connected to the PC",
        required=True,
    )
    parser.add_argument(
        "-b", "--baud", help="Router baud rate", default=115200, type=int
    )
    parser.add_argument(
        "-d",
        "--debug",
        help="Enable debug prints",
        action="store_const",
        const=True,
        default=False,
    )

    return parser.parse_args()


if "__main__" == __name__:
    sys.exit(main())
