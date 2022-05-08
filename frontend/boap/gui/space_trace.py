"""Handler of XY-plots of the ball position."""

from typing import Any, Dict, List, Tuple

import pyqtgraph as pg


class SpaceTrace(pg.PlotWidget):
    """Handler of XY-plots of the ball position."""

    POSITION_DOT_SIZE = 15
    POSITION_DOT_COLOR = (200, 200, 255)
    SETPOINT_DOT_SIZE = 5
    SETPOINT_DOT_COLOR = (0, 255, 200)

    class SpaceTraceImpl:
        """Ball position plotted in Cartesian coordinates."""

        def __init__(
            self,
            plot: pg.PlotWidget,
            history_length: int,
            dot_size: int,
            dot_color: Tuple[int, int, int],
        ) -> None:
            """Initialize an XY-plot."""
            self.history_length = history_length
            self.history: List[Dict[str, Any]] = []
            self.scatter_plot_item = pg.ScatterPlotItem(
                size=dot_size, brush=pg.mkBrush(color=dot_color)
            )
            plot.addItem(self.scatter_plot_item)

        def update(self, x: float, y: float) -> None:
            """Update the plot."""
            if len(self.history) < self.history_length:
                self.history.append({"pos": [x, y], "data": 1})
            else:
                self.history[:-1] = self.history[1:]
                self.history[-1] = {"pos": [x, y], "data": 1}
            self.scatter_plot_item.clear()
            self.scatter_plot_item.addPoints(self.history)

    def __init__(self, history_length: int) -> None:
        """Initialize an XY-plot handler."""
        super().__init__()
        self.position_trace = self.SpaceTraceImpl(
            plot=self,
            history_length=history_length,
            dot_size=self.POSITION_DOT_SIZE,
            dot_color=self.POSITION_DOT_COLOR,
        )
        self.setpoint_trace = self.SpaceTraceImpl(
            plot=self,
            history_length=history_length,
            dot_size=self.SETPOINT_DOT_SIZE,
            dot_color=self.SETPOINT_DOT_COLOR,
        )
        self.setLabel(axis="bottom", text="x", units="mm")
        self.setLabel(axis="left", text="y", units="mm")

        self.setXRange(-322 / 2, 322 / 2)
        self.setYRange(-247 / 2, 247 / 2)
        self.setMouseEnabled(False, False)

    def update(
        self,
        setpoint_x: float,
        position_x: float,
        setpoint_y: float,
        position_y: float,
    ) -> None:
        """Update the plots."""
        self.setpoint_trace.update(setpoint_x, setpoint_y)
        self.position_trace.update(position_x, position_y)
