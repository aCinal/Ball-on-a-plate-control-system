"""Common definitions to be used across the application."""


class BoapAcpTransactionError(Exception):
    """ACP transaction error."""

    ...


class BoapAcpInvalidMsgSizeError(Exception):
    """Invalid ACP message size error."""

    ...


class BoapAcpMalformedMessageError(Exception):
    """Malformed ACP message error."""

    ...


class PlantSettings:
    """Plant settings."""

    def __init__(self) -> None:
        """Construct default plant settings."""
        self.SamplingPeriod = 0.05
        self.XAxis = self.AxisSettings()
        self.YAxis = self.AxisSettings()

    class AxisSettings:
        """Per-axis settings."""

        def __init__(self) -> None:
            """Construct default per-axis settings."""
            self.ProportionalGain = 0.0
            self.IntegralGain = 0.0
            self.DerivativeGain = 0.0
            self.FilterOrder = 5


# C-compatible integer representation of boolean values
EBoapBool = int
EBoapBool_BoolFalse = 0
EBoapBool_BoolTrue = 1

# Return codes of BOAP APIs
EBoapRet = int
EBoapRet_Ok = 0
EBoapRet_Error = 1

# BOAP axis identifiers
EBoapAxis = int
EBoapAxis_X = 0
EBoapAxis_Y = 1
