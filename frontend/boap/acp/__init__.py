"""BOAP ACP utility."""

from typing import Any

from .stack import BoapAcp

singleton_stack = None


def initialize_acp_stack(port: Any, baud: int) -> None:
    """Initialize the ACP stack."""
    global singleton_stack
    singleton_stack = BoapAcp(port, baud)


def get_acp_stack_handle() -> BoapAcp:
    """Fetch an initialized ACP stack handle."""
    assert (
        singleton_stack
    ), "Tried fetching ACP stack handle before initialization"
    return singleton_stack
