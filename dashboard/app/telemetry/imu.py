import numpy as np

from bokeh.models import ColumnDataSource, Span, WheelZoomTool, CrosshairTool
from bokeh.plotting import figure
from bokeh.palettes import Spectral11

from app.telemetry.psst import Telemetry


def imu_figure(telemetry: Telemetry, lod: int, markers: list[float]) -> figure:
    FRAME_COLOR = '#808080'
    FRONT_COLOR = Spectral11[1]
    REAR_COLOR = Spectral11[2]

    # Determine max length among present IMUs to calculate time array
    imu_len = 0
    if telemetry.IMUFrame.Present:
        imu_len = len(telemetry.IMUFrame.Ax)
    elif telemetry.IMUFork.Present:
        imu_len = max(imu_len, len(telemetry.IMUFork.Ax))
    elif telemetry.IMURear.Present:
        imu_len = max(imu_len, len(telemetry.IMURear.Ax))

    time = np.around(np.arange(0, imu_len, lod) / telemetry.IMUSampleRate, 4) if imu_len > 0 else np.array([])

    data = dict(t=time)

    def process_imu(imu, prefix):
        if imu.Present:
            # Convert to G
            ax_g = np.array(imu.Ax) / imu.AccelLsbPerG
            ay_g = np.array(imu.Ay) / imu.AccelLsbPerG
            az_g = np.array(imu.Az) / imu.AccelLsbPerG

            # Magnitude
            mag = np.sqrt(ax_g**2 + ay_g**2 + az_g**2)

            # LOD decimation
            data[f'{prefix}_mag'] = mag[::lod]
            data[f'{prefix}_ax'] = ax_g[::lod]
            data[f'{prefix}_ay'] = ay_g[::lod]
            data[f'{prefix}_az'] = az_g[::lod]
            return True
        else:
            if imu_len > 0:
                zeros = np.zeros(len(time))
                data[f'{prefix}_mag'] = zeros
                data[f'{prefix}_ax'] = zeros
                data[f'{prefix}_ay'] = zeros
                data[f'{prefix}_az'] = zeros
            return False

    frame_present = process_imu(telemetry.IMUFrame, "frame")
    fork_present = process_imu(telemetry.IMUFork, "fork")
    rear_present = process_imu(telemetry.IMURear, "rear")

    source = ColumnDataSource(data=data)

    p = figure(
        name='imu',
        title="Accelerometer (G)",
        height=300,
        min_border_left=50,
        min_border_right=50,
        sizing_mode="stretch_width",
        toolbar_location='above',
        tools='xpan,reset,hover',
        active_inspect=None,
        active_drag='xpan',
        x_axis_label="Elapsed time (s)",
        y_axis_label="Acceleration (G)",
        output_backend='webgl')

    tooltips = [("elapsed time", "@t s")]
    if frame_present:
        tooltips.append(("frame", "@frame_mag G (ax: @frame_ax, ay: @frame_ay, az: @frame_az)"))
        p.line('t', 'frame_mag', legend_label="Frame", line_width=1, color=FRAME_COLOR, source=source)
    if fork_present:
        tooltips.append(("fork", "@fork_mag G (ax: @fork_ax, ay: @fork_ay, az: @fork_az)"))
        p.line('t', 'fork_mag', legend_label="Fork", line_width=1, color=FRONT_COLOR, source=source)
    if rear_present:
        tooltips.append(("rear", "@rear_mag G (ax: @rear_ax, ay: @rear_ay, az: @rear_az)"))
        p.line('t', 'rear_mag', legend_label="Rear", line_width=1, color=REAR_COLOR, source=source)

    p.hover.tooltips = tooltips

    if markers:
        for marker in markers:
            p.add_layout(Span(location=marker, dimension='height',
                              line_color='red', line_dash='dashed',
                              line_width=2))

    wz = WheelZoomTool(maintain_focus=False, dimensions='width')
    p.add_tools(wz)
    p.toolbar.active_scroll = wz

    s_current_time = Span(name='s_current_time',
                          location=0,
                          dimension='height',
                          line_color='#d0d0d0')
    ch = CrosshairTool(dimensions='height', line_color='#d0d0d0',
                       overlay=s_current_time)
    p.add_tools(ch)
    p.toolbar.active_inspect = ch
    p.hover.mode = 'vline'

    p.legend.location = 'bottom_right'
    p.legend.click_policy = 'hide'

    return p
