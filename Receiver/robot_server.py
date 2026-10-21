""" robot_server.py
Script to visualize telemetry sent by the robot
Copyright (c) 2026 Diego Quesada
All rights reserved.

This software is licensed under terms that can be found in the LICENSE file
in the root directory of this software component.
If no LICENSE file comes with this software, it is provided AS-IS.
"""

import socket
import sys
from dataclasses import dataclass
from typing import Optional
from PyQt6 import QtWidgets, QtCore, QtGui

@dataclass
class TelemetryData:
    changed: bool
    state: str
    distance: int
    speed_left: int
    speed_right: int
    direction: float

def parse_telemetry(line: str) -> Optional[TelemetryData]:
    """
    Parses telemetry data string.
    Format: [change][state] [distance] [speedL] [speedR] [direction]
    Example: "* > 150 1000 1000 45" or " | 1000 0 0 0"
    """
    try:
        if len(line) < 2:
            return None

        changed = line[0] == '*'
        state = line[2]

        # Split the rest of the line by whitespace
        parts = line[4:].strip().split()
        if len(parts) < 4:
            return None

        return TelemetryData(
            changed=changed,
            state=state,
            distance=int(parts[0]),
            speed_left=int(parts[1]),
            speed_right=int(parts[2]),
            direction=float(parts[3])
        )
    except (ValueError, IndexError):
        print(f"Value or Index error parsing line {line}")
        return None
    except Exception as e:
        # Handle any other unexpected format mismatch errors
        print(f"Error parsing line '{line}': {e}")
        return None

class TelemetryWorker(QtCore.QThread):
    """Handles connections from the robot over TCP in a background thread."""
    data_received = QtCore.pyqtSignal(TelemetryData, str)
    status_message = QtCore.pyqtSignal(str)

    def run(self):
        server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        server_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

        try:
            server_socket.bind(('0.0.0.0', 9576))
            server_socket.listen(1)
            self.status_message.emit("Listening on port 9576...")
        except socket.error as e:
            self.status_message.emit(f"Error binding socket: {e}")
            return

        try:
            while True:
                try:
                    client_socket, client_address = server_socket.accept()
                    self.status_message.emit(f"Connected by {client_address}")
                    client_socket.settimeout(30.0)

                    buffer = ""
                    while True:
                        try:
                            data = client_socket.recv(1024)
                            if not data:
                                break

                            buffer += data.decode('utf-8', errors='replace')
                            while '\n' in buffer:
                                line, buffer = buffer.split('\n', 1)
                                telemetry = parse_telemetry(line)
                                if telemetry:
                                    self.data_received.emit(telemetry, line)

                        except socket.timeout:
                            break
                        except Exception as e:
                            self.status_message.emit(f"Read error: {e}")
                            break

                    client_socket.close()
                    self.status_message.emit("Client disconnected. Waiting for reconnect...")
                except Exception as e:
                    self.status_message.emit(f"Accept error: {e}")
        finally:
            server_socket.close()

class CompassWidget(QtWidgets.QWidget):
    """Custom widget to draw the robot's heading and state."""
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(300, 300)
        self.direction = 0
        self.state = '|'

    def update_data(self, direction, state):
        self.direction = direction
        self.state = state
        self.update()

    def paintEvent(self, event):
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing)

        # Center and radius
        rect = self.rect()
        center = QtCore.QPointF(rect.width() / 2, rect.height() / 2)
        radius = min(rect.width(), rect.height()) / 2 - 20

        # Draw outer circle (Compass)
        painter.setPen(QtGui.QPen(QtCore.Qt.GlobalColor.black, 3))
        painter.drawEllipse(QtCore.QPointF(center), radius, radius)

        # Draw North indicator
        painter.setPen(QtGui.QPen(QtCore.Qt.GlobalColor.red, 2))
        painter.drawLine(int(center.x()), int(center.y() - radius), int(center.x()), int(center.y() - radius - 10))
        painter.drawText(rect.adjusted(0, 0, 0, -rect.height() + 10), QtCore.Qt.AlignmentFlag.AlignCenter | QtCore.Qt.AlignmentFlag.AlignTop, "N")

        # Draw Direction Arrow (Robot Heading)
        painter.save()
        painter.translate(center)
        painter.rotate(self.direction)

        painter.setPen(QtGui.QPen(QtCore.Qt.GlobalColor.blue, 4))
        painter.drawLine(0, 0, 0, int(-radius + 20))
        # Arrow head
        painter.drawLine(0, int(-radius + 20), -10, int(-radius + 30))
        painter.drawLine(0, int(-radius + 20), 10, int(-radius + 30))
        painter.restore()

        state_text = {
            '|': "***",
            '>': "Drive",
            '\\': "Stop",
            '^': "Turn",
            '/': "Stop",
            '<': "Reverse"
        }
        
        painter.setPen(QtGui.QPen(QtCore.Qt.GlobalColor.darkGreen, 5))
        painter.setFont(QtGui.QFont("Arial", 24, QtGui.QFont.Weight.Bold))
        painter.drawText(rect.adjusted(0, 0, 0, -(int(rect.height() / 2))),
                         QtCore.Qt.AlignmentFlag.AlignCenter | QtCore.Qt.AlignmentFlag.AlignVCenter,
                         state_text[self.state])

class TelemetryGUI(QtWidgets.QMainWindow):
    """Main Window for Robot Telemetry."""
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Cherokey3 Robot Telemetry")

        # Main Layout
        central_widget = QtWidgets.QWidget()
        self.setCentralWidget(central_widget)
        layout = QtWidgets.QVBoxLayout(central_widget)

        # Compass
        self.compass = CompassWidget()
        layout.addWidget(self.compass, alignment=QtCore.Qt.AlignmentFlag.AlignCenter)

        # Stats Panel
        stats_layout = QtWidgets.QHBoxLayout()

        self.lbl_speed_l = QtWidgets.QLabel("Speed Left: 0")
        self.lbl_speed_r = QtWidgets.QLabel("Speed Right: 0")
        self.lbl_dist = QtWidgets.QLabel("Distance: 0 mm")

        # Styling for labels
        font = QtGui.QFont("Consolas", 14)
        for lbl in [self.lbl_speed_l, self.lbl_speed_r, self.lbl_dist]:
            lbl.setFont(font)
            stats_layout.addWidget(lbl)

        layout.addLayout(stats_layout)

        # Raw Data Log
        log_label = QtWidgets.QLabel("Raw Telemetry Log:")
        log_label.setFont(QtGui.QFont("Arial", 10, QtGui.QFont.Weight.Bold))
        layout.addWidget(log_label)

        self.log_window = QtWidgets.QPlainTextEdit()
        self.log_window.setReadOnly(True)
        self.log_window.setMinimumHeight(150)
        self.log_window.setFont(QtGui.QFont("Consolas", 10))

        # Log window with its action buttons on the right
        log_layout = QtWidgets.QHBoxLayout()

        btn_clear = QtWidgets.QPushButton("Clear")
        btn_clear.setToolTip("Clear the telemetry log")
        btn_clear.clicked.connect(self.clear_log)

        btn_save = QtWidgets.QPushButton("Save")
        btn_save.setToolTip("Save the telemetry log to a file")
        btn_save.clicked.connect(self.save_log)

        button_layout = QtWidgets.QVBoxLayout()
        button_layout.addWidget(btn_clear)
        button_layout.addWidget(btn_save)
        button_layout.addStretch()

        log_layout.addWidget(self.log_window, stretch=1)
        log_layout.addLayout(button_layout)
        layout.addLayout(log_layout)

        # Status Bar
        self.statusBar().showMessage("Initializing...")

        # Worker Thread
        self.worker = TelemetryWorker()
        self.worker.data_received.connect(self.on_data_received)
        self.worker.status_message.connect(self.on_status_update)
        self.worker.start()

    def on_data_received(self, data: TelemetryData, raw_line: str):
        self.compass.update_data(data.direction, data.state)
        self.lbl_speed_l.setText(f"Speed Left: {data.speed_left}")
        self.lbl_speed_r.setText(f"Speed Right: {data.speed_right}")
        self.lbl_dist.setText(f"Distance: {data.distance} mm")

        # Append raw line to log window
        self.log_window.appendPlainText(raw_line.removesuffix('\r'))
        # Auto-scroll to bottom
        self.log_window.verticalScrollBar().setValue(self.log_window.verticalScrollBar().maximum())

    def on_status_update(self, message):
        self.statusBar().showMessage(message)

    def clear_log(self):
        """Clears the contents of the telemetry log."""
        self.log_window.clear()
        self.statusBar().showMessage("Log cleared", 3000)

    def save_log(self):
        """Saves the contents of the telemetry log to a file."""
        filename, _ = QtWidgets.QFileDialog.getSaveFileName(
            self,
            "Save Telemetry Log",
            "telemetry_log.txt",
            "Text files (*.txt);;All files (*)"
        )
        if not filename:
            return

        try:
            with open(filename, 'w', encoding='utf-8') as f:
                f.write(self.log_window.toPlainText())
        except OSError as e:
            QtWidgets.QMessageBox.critical(self, "Save Error", f"Could not save file:\n{e}")
            self.statusBar().showMessage(f"Error saving log: {e}", 5000)
            return

        self.statusBar().showMessage(f"Log saved to {filename}", 5000)

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    gui = TelemetryGUI()
    gui.show()
    sys.exit(app.exec())
