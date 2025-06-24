import sys
import os
import json
import subprocess
from PyQt5.QtWidgets import (QApplication, QMainWindow, QTabWidget, QWidget, QVBoxLayout, 
                            QHBoxLayout, QTableWidget, QTableWidgetItem, QTextEdit, 
                            QLabel, QPushButton, QFileDialog, QGridLayout, QGroupBox,
                            QHeaderView, QSplitter, QComboBox, QMessageBox, QProgressBar,
                            QCheckBox, QSlider, QDialog, QRadioButton, QLineEdit)
from PyQt5.QtCore import Qt, QThread, pyqtSignal
from PyQt5.QtGui import QFont, QColor, QTextCharFormat, QBrush, QPainter, QPen

# Dark theme color palette
DARK_BG = "#1e1e1e"
LIGHTER_BG = "#252526"
LIGHTEST_BG = "#2d2d30"
TEXT_COLOR = "#e1e1e1"
ACCENT_COLOR = "#0078d7"
ACCENT_HOVER = "#1c97ff"
BUTTON_RUN = "#28a745"  # Green
BUTTON_STEP = "#0078d7"  # Blue
BUTTON_CONTROL = "#dc3545"  # Red
CHECKBOX_COLOR = "#ffc107"  # Amber/Yellow
HIGHLIGHT_COLOR = "#5D3E8D"  # Dark Purple for highlighting

class SimulatorThread(QThread):
    """Thread for running simulator to avoid freezing GUI"""
    finished = pyqtSignal(bool, str)
    progress = pyqtSignal(str)
    
    def __init__(self, executable_path, input_file, 
                 pipelining=True, 
                 forwarding=True, 
                 print_reg_each_cycle=False, 
                 print_pipeline_reg=True,
                 print_bp_info=False,
                 save_snapshots=False,
                 trace_inst=False,
                 trace_inst_num=None,
                 trace_inst_pc=None,
                 step_mode=False,
                 single_step=False):
        super().__init__()
        self.executable_path = executable_path
        self.input_file = input_file
        self.pipelining = pipelining
        self.forwarding = forwarding
        self.print_reg_each_cycle = print_reg_each_cycle
        self.print_pipeline_reg = print_pipeline_reg
        self.print_bp_info = print_bp_info
        self.save_snapshots = save_snapshots
        self.trace_inst = trace_inst
        self.trace_inst_num = trace_inst_num
        self.trace_inst_pc = trace_inst_pc
        self.step_mode = step_mode
        self.single_step = single_step
        
    def run(self):
        try:
            cmd = [self.executable_path, "--input", self.input_file]
            
            # Configure the pipeline mode
            if not self.pipelining:
                cmd.append("--no-pipeline")
                
            # Configure data forwarding
            if not self.forwarding:
                cmd.append("--no-forwarding")
                
            # Configure register printing
            if self.print_reg_each_cycle:
                cmd.append("--print-registers")
                
            # Configure pipeline register printing - only pass when explicitly enabled
            if self.print_pipeline_reg:
                cmd.append("--print-pipeline")
            else:
                # Add flag to disable pipeline register printing if available
                cmd.append("--no-print-pipeline")
                
            # Configure branch predictor info printing
            if self.print_bp_info:
                cmd.append("--print-bp")
                
            # Configure snapshot saving
            if self.save_snapshots:
                cmd.append("--save-snapshots")  
            # Configure instruction tracing
            if self.trace_inst:
                if self.trace_inst_num is not None:
                    cmd.append("--trace")
                    cmd.append(str(self.trace_inst_num))
                elif self.trace_inst_pc is not None:
                    cmd.append("--trace")
                    cmd.append(hex(self.trace_inst_pc))
                    
                # Log the trace configuration
                self.progress.emit(f"Tracing enabled: {'Instruction #' + str(self.trace_inst_num) if self.trace_inst_num is not None else 'PC ' + hex(self.trace_inst_pc) if self.trace_inst_pc is not None else 'No value specified'}")
            
            # Configure step mode
            if self.step_mode:
                cmd.append("--step")
                
                
            self.progress.emit(f"Running command: {' '.join(cmd)}")
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            
            # Stream output in real time
            while True:
                output = process.stdout.readline()
                if output == '' and process.poll() is not None:
                    break
                if output:
                    self.progress.emit(output.strip())
            
            # Get the return code
            return_code = process.poll()
            
            # Get any error output
            stderr_output = process.stderr.read()
            
            if return_code == 0:
                self.finished.emit(True, "Simulation completed successfully.")
            else:
                self.finished.emit(False, f"Simulation failed with error code {return_code}. Error: {stderr_output}")
                
        except Exception as e:
            self.finished.emit(False, f"Error running simulation: {str(e)}")

class PipelineVisualizerWidget(QWidget):
    """Widget for visualizing the pipeline state"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.snapshot = None
        self.setMinimumHeight(200)
        # Dark background for the visualizer
        self.setStyleSheet(f"background-color: {LIGHTEST_BG}; border-radius: 8px;")
        
    def setSnapshot(self, snapshot):
        """Set the pipeline snapshot to visualize"""
        self.snapshot = snapshot
        self.update()  # Trigger a repaint
        
    def paintEvent(self, event):
        """Draw the pipeline visualization"""
        if not self.snapshot:
            return
            
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # Set up fonts
        title_font = QFont("Arial", 11, QFont.Bold)
        stage_font = QFont("Arial", 10, QFont.Bold)
        content_font = QFont("Arial", 9)
        
        # Set up colors - using higher contrast colors for dark theme
        valid_color = QColor(60, 180, 75)     # Bright green for valid stages
        bubble_color = QColor(80, 80, 80)     # Dark gray for bubbles
        text_color = QColor(255, 255, 255)    # White for text
        border_color = QColor(120, 120, 120)  # Medium gray for borders
        highlight_color = QColor(210, 105, 30) # Orange for highlights
        forwarding_color = QColor(220, 70, 70) # Red for forwarding lines
        
        # Get widget dimensions
        width = self.width()
        height = self.height()
        
        # Calculate stage box dimensions
        stage_width = int(width * 0.18)
        stage_height = int(height * 0.6)
        stage_spacing = int((width - 5 * stage_width) / 6)
        stage_y = int(height * 0.2)
        
        # Draw title with custom font
        painter.setFont(title_font)
        painter.setPen(QPen(text_color, 2))
        painter.drawText(int(width/2 - 120), 25, f"Pipeline State - Cycle {self.snapshot.get('cycle', 0)}")
        
        # Draw the pipeline stages
        stages = [
            {"name": "IF", "valid": self.snapshot.get("if_id_valid", False), 
             "pc": self.snapshot.get("if_id_pc", 0),
             "info": self.snapshot.get("if_id_inst", 0)},
            {"name": "ID", "valid": self.snapshot.get("id_ex_valid", False), 
             "pc": self.snapshot.get("id_ex_pc", 0),
             "info": self.snapshot.get("id_ex_type", "")},
            {"name": "EX", "valid": self.snapshot.get("ex_mem_valid", False), 
             "pc": self.snapshot.get("ex_mem_pc", 0),
             "info": self.snapshot.get("ex_mem_result", 0)},
            {"name": "MEM", "valid": self.snapshot.get("mem_wb_valid", False), 
             "pc": self.snapshot.get("mem_wb_pc", 0),
             "info": self.snapshot.get("mem_wb_data", "")},
            {"name": "WB", "valid": self.snapshot.get("wb_valid", False), 
             "pc": self.snapshot.get("wb_pc", 0),
             "info": self.snapshot.get("wb_result", "")}
        ]
        
        # Stage colors for different pipeline stages
        stage_colors = [
            QColor(50, 100, 200),  # IF - Blue
            QColor(50, 150, 100),  # ID - Green
            QColor(150, 100, 50),  # EX - Brown
            QColor(150, 50, 150),  # MEM - Purple
            QColor(200, 50, 50)    # WB - Red
        ]
        
        # Draw each stage
        for i, stage in enumerate(stages):
            x = stage_spacing + i * (stage_width + stage_spacing)
            
            # Draw the box with gradient
            if stage["valid"]:
                # Use stage-specific color
                color = stage_colors[i]
                painter.setBrush(QBrush(color))
            else:
                painter.setBrush(QBrush(bubble_color))
            
            # Draw border with rounded corners
            painter.setPen(QPen(border_color, 2))
            painter.drawRoundedRect(x, stage_y, stage_width, stage_height, 10, 10)
            
            # Draw the stage name with better font
            painter.setPen(QPen(text_color, 2))
            painter.setFont(stage_font)
            painter.drawText(int(x + stage_width/2 - 15), stage_y + 25, stage["name"])
            
            # Draw PC and info if valid
            if stage["valid"]:
                painter.setFont(content_font)
                
                # Draw PC with consistent formatting
                pc_text = f"PC: 0x{stage['pc']:08x}"
                painter.drawText(x + 10, stage_y + 50, pc_text)
                
                # Draw additional info with better formatting
                if isinstance(stage["info"], int):
                    info_text = f"0x{stage['info']:08x}"
                else:
                    info_text = str(stage["info"])
                    
                if len(info_text) > 15:
                    info_text = info_text[:12] + "..."
                    
                painter.drawText(x + 10, stage_y + 75, info_text)
            
        # Draw data forwarding paths if active
        forwarding_paths = self.snapshot.get("forwarding_paths", [])
        painter.setPen(QPen(forwarding_color, 2, Qt.DashLine))
        
        for path in forwarding_paths:
            from_stage = path.get("from", 0)
            to_stage = path.get("to", 0)
            
            if 0 <= from_stage < 5 and 0 <= to_stage < 5:
                from_x = stage_spacing + from_stage * (stage_width + stage_spacing) + int(stage_width/2)
                to_x = stage_spacing + to_stage * (stage_width + stage_spacing) + int(stage_width/2)
                
                # Draw curved arc instead of straight line
                path_height = stage_y + stage_height + 15
                control_y = path_height + 20
                
                # Draw bezier curve for the forwarding path
                painter.drawLine(from_x, path_height, to_x, path_height)
                
                # Draw arrows at the end
                painter.drawLine(to_x, path_height, to_x - 5, path_height - 5)
                painter.drawLine(to_x, path_height, to_x - 5, path_height + 5)
                
                # Label with value
                painter.setFont(content_font)
                label = f"{path.get('value', '')}"
                painter.drawText(int((from_x + to_x)/2 - 15), path_height + 15, label)

class SimulatorGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("RISC-V Processor Simulator GUI")
        self.setGeometry(100, 100, 1200, 800)
        
        # Apply dark theme
        self.apply_dark_theme()
        
        # Create main widget and layout
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        self.main_layout = QVBoxLayout(main_widget)
        
        # Add control panel at the top
        self.create_control_panel()
        
        # Add simulator options panel
        self.create_options_panel()
        
        # Create tab widget
        self.tabs = QTabWidget()
        self.main_layout.addWidget(self.tabs)
        
        # Create visualization area for pipeline
        self.create_pipeline_visualizer()
        
        # Create output log at the bottom
        output_log_label = QLabel("Simulation Output Log:")
        output_log_label.setStyleSheet(f"color: {TEXT_COLOR}; font-weight: bold; margin-top: 5px;")
        self.output_log = QTextEdit()
        self.output_log.setReadOnly(True)
        self.output_log.setMaximumHeight(150)
        self.output_log.setStyleSheet(f"""
            QTextEdit {{
                background-color: {LIGHTER_BG};
                color: {TEXT_COLOR};
                border: 1px solid #444444;
                border-radius: 8px;
                padding: 5px;
                font-family: "Courier New", monospace;
            }}
        """)
        self.main_layout.addWidget(output_log_label)
        self.main_layout.addWidget(self.output_log)
        
        # Create tabs
        self.create_stats_tab()
        self.create_registers_tab()
        self.create_memory_tab()
        self.create_bp_tab()
        self.create_snapshots_tab()
        
        # Data placeholders
        self.stats_data = {}
        self.register_data = {}
        self.data_memory = {}
        self.stack_memory = {}
        self.bp_data = []
        self.snapshots = []
        self.current_snapshot_index = 0
        
        # Set the base directory
        self.base_dir = os.path.abspath(os.path.dirname(__file__))
        executable_name = "simulator"
        if sys.platform == "win32": # Add .exe extension on Windows
            executable_name += ".exe"
        self.executable_path = os.path.join(self.base_dir, executable_name)
        self.source_path = os.path.join(self.base_dir, "Newcursor.cpp")
        
        # Simulator thread
        self.sim_thread = None
        
        # Populate test files dropdown
        self.populate_test_files()
    
    def apply_dark_theme(self):
        """Apply dark theme to the entire application"""
        qss = f"""
        QWidget {{
            background-color: {DARK_BG};
            color: {TEXT_COLOR};
            font-family: Arial;
        }}
        
        QMainWindow {{
            background-color: {DARK_BG};
        }}
        
        QTabWidget::pane {{
            border: 1px solid #444444;
            background-color: {LIGHTER_BG};
            border-radius: 8px;
        }}
        
        QTabBar::tab {{
            background-color: {LIGHTER_BG};
            color: {TEXT_COLOR};
            border: 1px solid #444444;
            padding: 6px 12px;
            margin-right: 2px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }}
        
        QTabBar::tab:selected {{
            background-color: {LIGHTEST_BG};
            border-bottom: 2px solid {ACCENT_COLOR};
        }}
        
        QTabBar::tab:hover {{
            background-color: #3a3a3c;
        }}
        
        QGroupBox {{
            background-color: {LIGHTER_BG};
            border: 1px solid #444444;
            border-radius: 8px;
            margin-top: 1.5ex;
            font-weight: bold;
            padding: 10px;
        }}
        
        QGroupBox::title {{
            subcontrol-origin: margin;
            subcontrol-position: top center;
            padding: 0 5px;
            color: {TEXT_COLOR};
        }}
        
        QPushButton {{
            background-color: {LIGHTEST_BG};
            color: {TEXT_COLOR};
            border: 1px solid #555555;
            border-radius: 8px;
            padding: 6px 12px;
            font-weight: bold;
        }}
        
        QPushButton:hover {{
            background-color: #3e3e42;
            border: 1px solid {ACCENT_COLOR};
        }}
        
        QPushButton:pressed {{
            background-color: {ACCENT_COLOR};
        }}
        
        QPushButton:disabled {{
            background-color: #2a2a2a;
            color: #666666;
            border: 1px solid #3a3a3a;
        }}
        
        QComboBox {{
            background-color: {LIGHTEST_BG};
            color: {TEXT_COLOR};
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 4px 8px;
            min-height: 25px;
        }}
        
        QComboBox:hover {{
            border: 1px solid {ACCENT_COLOR};
        }}
        
        QComboBox::drop-down {{
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 20px;
            border-left: 1px solid #555555;
            border-top-right-radius: 4px;
            border-bottom-right-radius: 4px;
        }}
        
        QComboBox QAbstractItemView {{
            background-color: {LIGHTEST_BG};
            color: {TEXT_COLOR};
            selection-background-color: {ACCENT_COLOR};
            border: 1px solid #555555;
            border-radius: 4px;
        }}
        
        QLineEdit {{
            background-color: {LIGHTEST_BG};
            color: {TEXT_COLOR};
            border: 1px solid #555555;
            border-radius: 4px;
            padding: 4px 8px;
            min-height: 25px;
        }}
        
        QLineEdit:focus {{
            border: 1px solid {ACCENT_COLOR};
        }}
        
        QTextEdit {{
            background-color: {LIGHTER_BG};
            color: {TEXT_COLOR};
            border: 1px solid #444444;
            border-radius: 4px;
        }}
        
        QTableWidget {{
            background-color: {LIGHTER_BG};
            color: {TEXT_COLOR};
            gridline-color: #444444;
            border: 1px solid #444444;
            border-radius: 4px;
        }}
        
        QTableWidget::item:selected {{
            background-color: {ACCENT_COLOR};
            color: white;
        }}
        
        QHeaderView::section {{
            background-color: {LIGHTEST_BG};
            color: {TEXT_COLOR};
            border: 1px solid #444444;
            padding: 4px 8px;
            font-weight: bold;
        }}
        
        QSlider::groove:horizontal {{
            border: 1px solid #444444;
            height: 8px;
            background: {LIGHTER_BG};
            margin: 2px 0;
            border-radius: 4px;
        }}
        
        QSlider::handle:horizontal {{
            background: {ACCENT_COLOR};
            border: 1px solid {ACCENT_COLOR};
            width: 18px;
            height: 18px;
            margin: -5px 0;
            border-radius: 9px;
        }}
        
        QSlider::handle:horizontal:hover {{
            background: {ACCENT_HOVER};
        }}
        
        QCheckBox {{
            spacing: 8px;
        }}
        
        QCheckBox::indicator {{
            width: 18px;
            height: 18px;
            border: 2px solid {CHECKBOX_COLOR};
            border-radius: 4px;
        }}
        
        QCheckBox::indicator:unchecked {{
            background-color: {LIGHTER_BG};
        }}
        
        QCheckBox::indicator:checked {{
            background-color: {CHECKBOX_COLOR};
        }}
        
        QCheckBox::indicator:checked:disabled {{
            background-color: #666666;
        }}
        
        QRadioButton {{
            spacing: 8px;
        }}
        
        QRadioButton::indicator {{
            width: 18px;
            height: 18px;
            border: 2px solid {ACCENT_COLOR};
            border-radius: 9px;
        }}
        
        QRadioButton::indicator:unchecked {{
            background-color: {LIGHTER_BG};
        }}
        
        QRadioButton::indicator:checked {{
            background-color: {ACCENT_COLOR};
            border: 2px solid {ACCENT_COLOR};
        }}
        
        QProgressBar {{
            border: 1px solid #444444;
            border-radius: 4px;
            text-align: center;
            background-color: {LIGHTER_BG};
        }}
        
        QProgressBar::chunk {{
            background-color: {ACCENT_COLOR};
            width: 20px;
        }}
        
        QLabel {{
            color: {TEXT_COLOR};
        }}
        
        QScrollBar:vertical {{
            background: {LIGHTER_BG};
            width: 12px;
            margin: 12px 0 12px 0;
            border-radius: 6px;
        }}
        
        QScrollBar::handle:vertical {{
            background: #666666;
            min-height: 20px;
            border-radius: 6px;
        }}
        
        QScrollBar::handle:vertical:hover {{
            background: #777777;
        }}
        
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{
            background: none;
        }}
        
        QScrollBar:horizontal {{
            background: {LIGHTER_BG};
            height: 12px;
            margin: 0 12px 0 12px;
            border-radius: 6px;
        }}
        
        QScrollBar::handle:horizontal {{
            background: #666666;
            min-width: 20px;
            border-radius: 6px;
        }}
        
        QScrollBar::handle:horizontal:hover {{
            background: #777777;
        }}
        
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{
            background: none;
        }}
        """
        
        self.setStyleSheet(qss)
    
    def create_control_panel(self):
        """Create the control panel with buttons and dropdowns"""
        control_panel = QWidget()
        control_layout = QHBoxLayout(control_panel)
        
        # Test file selection dropdown
        self.file_label = QLabel("Select test file:")
        self.file_dropdown = QComboBox()
        
        # Compile button
        self.compile_button = QPushButton("Compile Simulator")
        self.compile_button.clicked.connect(self.compile_simulator)
        self.compile_button.setStyleSheet(f"background-color: {BUTTON_CONTROL}; border-radius: 8px;")
        
        # Step mode checkbox
        self.step_mode_checkbox = QCheckBox("Step Mode")
        self.step_mode_checkbox.setToolTip("Enable step-by-step execution")
        
        # Save snapshots checkbox
        self.save_snapshots_checkbox = QCheckBox("Save Snapshots")
        self.save_snapshots_checkbox.setToolTip("Save cycle-by-cycle snapshots")
        
        # Run button
        self.run_button = QPushButton("Run Simulation")
        self.run_button.clicked.connect(self.run_simulation)
        self.run_button.setStyleSheet(f"background-color: {BUTTON_RUN}; border-radius: 8px;")
        
        # Step button
        self.step_button = QPushButton("Step")
        self.step_button.clicked.connect(self.step_simulation)
        self.step_button.setToolTip("Run a single simulation step")
        self.step_button.setStyleSheet(f"background-color: {BUTTON_STEP}; border-radius: 8px;")
        
        # Clear button
        self.clear_button = QPushButton("Clear Results")
        self.clear_button.clicked.connect(self.clear_results)
        self.clear_button.setStyleSheet(f"background-color: {BUTTON_CONTROL}; border-radius: 8px;")
        
        # Progress bar
        self.progress_bar = QProgressBar()
        self.progress_bar.setTextVisible(False)
        self.progress_bar.setMaximum(0)  # Indeterminate progress
        self.progress_bar.setMinimum(0)
        self.progress_bar.hide()
        
        # Add to layout
        control_layout.addWidget(self.file_label)
        control_layout.addWidget(self.file_dropdown)
        control_layout.addWidget(self.compile_button)
        control_layout.addWidget(self.step_mode_checkbox)
        control_layout.addWidget(self.save_snapshots_checkbox)
        control_layout.addWidget(self.run_button)
        control_layout.addWidget(self.step_button)
        control_layout.addWidget(self.clear_button)
        control_layout.addWidget(self.progress_bar)
        
        self.main_layout.addWidget(control_panel)
    
    def create_options_panel(self):
        """Create the options panel with knobs and checkboxes"""
        options_panel = QGroupBox("Simulator Options")
        options_layout = QGridLayout(options_panel)
        
        # Set custom style for options panel
        options_panel.setStyleSheet(f"""
            QGroupBox {{
                background-color: {LIGHTER_BG};
                border: 1px solid #444444;
                border-radius: 8px;
                margin-top: 1.5ex;
                padding: 10px;
            }}
            QGroupBox::title {{
                subcontrol-origin: margin;
                subcontrol-position: top center;
                padding: 0 5px;
                color: {TEXT_COLOR};
                font-weight: bold;
            }}
        """)
        
        # Pipelining option
        self.pipelining_checkbox = QCheckBox("Pipelining Enabled")
        self.pipelining_checkbox.setChecked(True)
        self.pipelining_checkbox.setToolTip("Enable or disable pipeline execution")
        options_layout.addWidget(self.pipelining_checkbox, 0, 0)
        
        # Data forwarding option
        self.forwarding_checkbox = QCheckBox("Data Forwarding Enabled")
        self.forwarding_checkbox.setChecked(True)
        self.forwarding_checkbox.setToolTip("Enable or disable data forwarding")
        options_layout.addWidget(self.forwarding_checkbox, 0, 1)
        
        # Print registers option
        self.print_reg_checkbox = QCheckBox("Print Registers Each Cycle")
        self.print_reg_checkbox.setChecked(False)
        self.print_reg_checkbox.setToolTip("Print register values at each cycle")
        options_layout.addWidget(self.print_reg_checkbox, 1, 0)
        
        # Print pipeline registers option
        self.print_pipeline_checkbox = QCheckBox("Print Pipeline Registers")
        self.print_pipeline_checkbox.setChecked(False)  # Default to false
        self.print_pipeline_checkbox.setToolTip("Print pipeline register values in the log")
        options_layout.addWidget(self.print_pipeline_checkbox, 1, 1)
        
        # Print branch predictor info option
        self.print_bp_checkbox = QCheckBox("Print Branch Predictor Info")
        self.print_bp_checkbox.setChecked(False)
        self.print_bp_checkbox.setToolTip("Print branch predictor information")
        options_layout.addWidget(self.print_bp_checkbox, 2, 0)
        
        # Trace instruction option
        self.trace_inst_checkbox = QCheckBox("Trace Instruction")
        self.trace_inst_checkbox.setChecked(False)
        self.trace_inst_checkbox.setToolTip("Trace a specific instruction")
        options_layout.addWidget(self.trace_inst_checkbox, 2, 1)
        
        # Trace instruction number input
        trace_inst_layout = QHBoxLayout()
        self.trace_by_num_radio = QRadioButton("By Number:")
        self.trace_by_num_radio.setChecked(True)
        self.trace_inst_num_input = QLineEdit()
        self.trace_inst_num_input.setPlaceholderText("Inst #")
        self.trace_inst_num_input.setEnabled(False)
        trace_inst_layout.addWidget(self.trace_by_num_radio)
        trace_inst_layout.addWidget(self.trace_inst_num_input)
        options_layout.addLayout(trace_inst_layout, 3, 0)
        
        # Trace instruction PC input
        trace_pc_layout = QHBoxLayout()
        self.trace_by_pc_radio = QRadioButton("By PC:")
        self.trace_pc_input = QLineEdit()
        self.trace_pc_input.setPlaceholderText("PC (hex)")
        self.trace_pc_input.setEnabled(False)
        trace_pc_layout.addWidget(self.trace_by_pc_radio)
        trace_pc_layout.addWidget(self.trace_pc_input)
        options_layout.addLayout(trace_pc_layout, 3, 1)
        
        # Connect trace checkbox to enable/disable trace inputs
        self.trace_inst_checkbox.stateChanged.connect(self.update_trace_inputs)
        self.trace_by_num_radio.toggled.connect(self.update_trace_inputs)
        self.trace_by_pc_radio.toggled.connect(self.update_trace_inputs)
        
        self.main_layout.addWidget(options_panel)
    
    def update_trace_inputs(self):
        """Update the enabled state of trace input fields based on checkbox state"""
        trace_enabled = self.trace_inst_checkbox.isChecked()
        self.trace_by_num_radio.setEnabled(trace_enabled)
        self.trace_by_pc_radio.setEnabled(trace_enabled)
        
        # Enable the appropriate input based on radio button selection
        self.trace_inst_num_input.setEnabled(trace_enabled and self.trace_by_num_radio.isChecked())
        self.trace_pc_input.setEnabled(trace_enabled and self.trace_by_pc_radio.isChecked())
        
        # Log the change to help with debugging
        if trace_enabled:
            if self.trace_by_num_radio.isChecked():
                self.output_log.append("Tracing by instruction number enabled")
            else:
                self.output_log.append("Tracing by PC address enabled")
        else:
            self.output_log.append("Instruction tracing disabled")
    
    def create_pipeline_visualizer(self):
        """Create the pipeline visualization panel"""
        pipeline_group = QGroupBox("Pipeline Visualization")
        pipeline_layout = QVBoxLayout()
        
        # Set custom style for pipeline visualization
        pipeline_group.setStyleSheet(f"""
            QGroupBox {{
                background-color: {LIGHTER_BG};
                border: 1px solid #444444;
                border-radius: 8px;
                margin-top: 1.5ex;
                padding: 10px;
            }}
            QGroupBox::title {{
                subcontrol-origin: margin;
                subcontrol-position: top center;
                padding: 0 5px;
                color: {TEXT_COLOR};
                font-weight: bold;
            }}
        """)
        
        # Pipeline visualization widget
        self.pipeline_visualizer = PipelineVisualizerWidget()
        self.pipeline_visualizer.setStyleSheet(f"background-color: {LIGHTEST_BG}; border-radius: 8px;")
        
        # Step controls
        step_controls = QWidget()
        step_layout = QHBoxLayout(step_controls)
        
        self.prev_button = QPushButton("◀ Previous Cycle")
        self.prev_button.clicked.connect(self.prev_snapshot)
        self.prev_button.setEnabled(False)
        self.prev_button.setStyleSheet(f"background-color: {BUTTON_STEP}; border-radius: 8px;")
        
        self.next_button = QPushButton("Next Cycle ▶")
        self.next_button.clicked.connect(self.next_snapshot)
        self.next_button.setEnabled(False)
        self.next_button.setStyleSheet(f"background-color: {BUTTON_STEP}; border-radius: 8px;")
        
        self.cycle_slider = QSlider(Qt.Horizontal)
        self.cycle_slider.setMinimum(0)
        self.cycle_slider.setMaximum(0)
        self.cycle_slider.setValue(0)
        self.cycle_slider.setEnabled(False)
        self.cycle_slider.valueChanged.connect(self.slider_change_snapshot)
        
        self.cycle_label = QLabel("Cycle: 0/0")
        
        step_layout.addWidget(self.prev_button)
        step_layout.addWidget(self.cycle_slider)
        step_layout.addWidget(self.cycle_label)
        step_layout.addWidget(self.next_button)
        
        pipeline_layout.addWidget(self.pipeline_visualizer)
        pipeline_layout.addWidget(step_controls)
        
        pipeline_group.setLayout(pipeline_layout)
        self.main_layout.addWidget(pipeline_group)
    
    def slider_change_snapshot(self, value):
        """Handle slider value change"""
        if 0 <= value < len(self.snapshots):
            self.current_snapshot_index = value
            self.update_snapshot_display()
            
    def prev_snapshot(self):
        """Go to previous snapshot"""
        if self.current_snapshot_index > 0:
            self.current_snapshot_index -= 1
            self.cycle_slider.setValue(self.current_snapshot_index)
            self.update_snapshot_display()
            
    def next_snapshot(self):
        """Go to next snapshot"""
        if self.current_snapshot_index < len(self.snapshots) - 1:
            self.current_snapshot_index += 1
            self.cycle_slider.setValue(self.current_snapshot_index)
            self.update_snapshot_display()
            
    def update_snapshot_display(self):
        """Update all views to reflect the current snapshot"""
        if not self.snapshots:
            return
            
        snapshot = self.snapshots[self.current_snapshot_index]
        
        # Update cycle label
        self.cycle_label.setText(f"Cycle: {self.current_snapshot_index + 1}/{len(self.snapshots)}")
        
        # Update pipeline visualizer
        self.pipeline_visualizer.setSnapshot(self.convert_snapshot_for_visualizer(snapshot))
        
        # Update the snapshot details table
        self.snapshot_table.setRowCount(0)  # Clear existing rows
        self.populate_snapshot_table(snapshot)
        
        # Update enable/disable state of navigation buttons
        self.prev_button.setEnabled(self.current_snapshot_index > 0)
        self.next_button.setEnabled(self.current_snapshot_index < len(self.snapshots) - 1)
        
        # Print pipeline register values to output log if the option is enabled
        if hasattr(self, 'print_pipeline_checkbox') and self.print_pipeline_checkbox.isChecked():
            self.output_log.append(f"\n--- Pipeline Registers at Cycle {snapshot.get('clockCycles', 0)} ---")
            
            # IF/ID Register
            if_id = snapshot.get("if_id", {})
            if if_id.get("valid", False):
                self.output_log.append(f"IF/ID: PC={hex(if_id.get('pc', 0))}, Inst={hex(if_id.get('instruction', 0))}")
            else:
                self.output_log.append("IF/ID: <invalid>")
                
            # ID/EX Register
            id_ex = snapshot.get("id_ex", {})
            if id_ex.get("valid", False):
                self.output_log.append(f"ID/EX: PC={hex(id_ex.get('pc', 0))}, Type={id_ex.get('instType', '')}-{id_ex.get('subType', '')}")
            else:
                self.output_log.append("ID/EX: <invalid>")
                
            # EX/MEM Register
            ex_mem = snapshot.get("ex_mem", {})
            if ex_mem.get("valid", False):
                self.output_log.append(f"EX/MEM: PC={hex(ex_mem.get('pc', 0))}, Result={hex(ex_mem.get('aluResult', 0))}")
            else:
                self.output_log.append("EX/MEM: <invalid>")
                
            # MEM/WB Register
            mem_wb = snapshot.get("mem_wb", {})
            if mem_wb.get("valid", False):
                self.output_log.append(f"MEM/WB: PC={hex(mem_wb.get('pc', 0))}, Data={mem_wb.get('memData', 0)}")
            else:
                self.output_log.append("MEM/WB: <invalid>")
    
    def populate_snapshot_table(self, snapshot):
        """Populate the snapshot details table with the current snapshot data"""
        # Function to add a row to the snapshot table
        def add_row(property_name, value):
            row = self.snapshot_table.rowCount()
            self.snapshot_table.insertRow(row)
            self.snapshot_table.setItem(row, 0, QTableWidgetItem(property_name))
            
            # Format value based on its type
            if isinstance(value, int):
                value_str = f"0x{value:x} ({value})"
            elif isinstance(value, bool):
                value_str = str(value)
            else:
                value_str = str(value)
                
            self.snapshot_table.setItem(row, 1, QTableWidgetItem(value_str))
        
        # Add cycle number
        add_row("Cycle", snapshot.get("clockCycles", 0))
        
        # Add IF/ID stage details
        if_id = snapshot.get("if_id", {})
        add_row("IF/ID Valid", if_id.get("valid", False))
        if if_id.get("valid", False):
            add_row("IF/ID PC", if_id.get("pc", 0))
            add_row("IF/ID Instruction", if_id.get("instruction", 0))
        
        # Add ID/EX stage details
        id_ex = snapshot.get("id_ex", {})
        add_row("ID/EX Valid", id_ex.get("valid", False))
        if id_ex.get("valid", False):
            add_row("ID/EX PC", id_ex.get("pc", 0))
            add_row("ID/EX Instruction Type", id_ex.get("instType", ""))
            add_row("ID/EX Instruction SubType", id_ex.get("subType", ""))
        
        # Add EX/MEM stage details
        ex_mem = snapshot.get("ex_mem", {})
        add_row("EX/MEM Valid", ex_mem.get("valid", False))
        if ex_mem.get("valid", False):
            add_row("EX/MEM PC", ex_mem.get("pc", 0))
            add_row("EX/MEM ALU Result", ex_mem.get("aluResult", 0))
        
        # Add MEM/WB stage details
        mem_wb = snapshot.get("mem_wb", {})
        add_row("MEM/WB Valid", mem_wb.get("valid", False))
        if mem_wb.get("valid", False):
            add_row("MEM/WB PC", mem_wb.get("pc", 0))
            add_row("MEM/WB Data", mem_wb.get("memData", 0))
    
    def convert_snapshot_for_visualizer(self, snapshot):
        """Convert pipeline snapshot to the format expected by the visualizer"""
        converted = {
            "cycle": snapshot.get("clockCycles", 0),
            
            # IF/ID
            "if_id_valid": snapshot.get("if_id", {}).get("valid", False),
            "if_id_pc": snapshot.get("if_id", {}).get("pc", 0),
            "if_id_inst": snapshot.get("if_id", {}).get("instruction", 0),
            
            # ID/EX
            "id_ex_valid": snapshot.get("id_ex", {}).get("valid", False),
            "id_ex_pc": snapshot.get("id_ex", {}).get("pc", 0),
            "id_ex_type": f"{snapshot.get('id_ex', {}).get('instType', '')}-{snapshot.get('id_ex', {}).get('subType', '')}",
            
            # EX/MEM
            "ex_mem_valid": snapshot.get("ex_mem", {}).get("valid", False),
            "ex_mem_pc": snapshot.get("ex_mem", {}).get("pc", 0),
            "ex_mem_result": snapshot.get("ex_mem", {}).get("aluResult", 0),
            
            # MEM/WB
            "mem_wb_valid": snapshot.get("mem_wb", {}).get("valid", False),
            "mem_wb_pc": snapshot.get("mem_wb", {}).get("pc", 0),
            "mem_wb_data": snapshot.get("mem_wb", {}).get("memData", 0),
            
            # WB - directly from WB stage
            "wb_valid": snapshot.get("wb_valid", False),
            "wb_pc": snapshot.get("wb_pc", 0),
            "wb_result": snapshot.get("wb_result", 0),
            
            # Forwarding paths (extract from the snapshot if available)
            "forwarding_paths": []
        }
        
        return converted
    
    def create_snapshots_tab(self):
        """Create the tab for viewing pipeline snapshots"""
        snapshots_tab = QWidget()
        snapshots_layout = QVBoxLayout(snapshots_tab)
        snapshots_layout.setContentsMargins(10, 10, 10, 10)
        snapshots_layout.setSpacing(10)
        
        # Table for displaying snapshot properties
        self.snapshot_table = QTableWidget()
        self.snapshot_table.setColumnCount(2)
        self.snapshot_table.setHorizontalHeaderLabels(["Property", "Value"])
        self.snapshot_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
        self.snapshot_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.Stretch)
        self.snapshot_table.setStyleSheet(f"""
            QTableWidget {{
                background-color: {LIGHTEST_BG};
                color: {TEXT_COLOR};
                gridline-color: #555555;
                border: 1px solid #444444;
                border-radius: 8px;
            }}
            QTableWidget::item {{
                padding: 4px;
            }}
            QTableWidget::item:selected {{
                background-color: {ACCENT_COLOR};
                color: white;
            }}
        """)
        
        # Button to view raw log file
        self.view_log_button = QPushButton("View Raw Log File")
        self.view_log_button.clicked.connect(self.view_raw_log_file)
        self.view_log_button.setStyleSheet(f"background-color: {BUTTON_CONTROL}; border-radius: 8px;")
        
        snapshots_layout.addWidget(self.snapshot_table)
        snapshots_layout.addWidget(self.view_log_button)
        
        self.tabs.addTab(snapshots_tab, "Pipeline Snapshots")
    
    def view_raw_log_file(self):
        """Display the raw content of cycle_snapshots.log in a dialog"""
        log_path = os.path.join(self.base_dir, "cycle_snapshots.log")
        
        if not os.path.exists(log_path):
            QMessageBox.information(self, "File Not Found", "cycle_snapshots.log file not found")
            return
            
        try:
            with open(log_path, 'r') as f:
                content = f.read()
                
            # Create a custom dialog with a text edit to display the content
            dialog = QDialog(self)
            dialog.setWindowTitle("cycle_snapshots.log Contents")
            dialog.resize(700, 500)
            
            layout = QVBoxLayout(dialog)
            
            text_edit = QTextEdit()
            text_edit.setReadOnly(True)
            text_edit.setPlainText(content)
            text_edit.setLineWrapMode(QTextEdit.NoWrap)
            
            layout.addWidget(text_edit)
            
            # Add a close button
            close_button = QPushButton("Close")
            close_button.clicked.connect(dialog.accept)
            layout.addWidget(close_button)
            
            dialog.exec_()
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Error reading log file: {str(e)}")
    
    def populate_test_files(self):
        """Find and populate test files in the dropdown"""
        test_dir = self.base_dir

        if os.path.exists(test_dir):
            test_files = [f for f in os.listdir(test_dir) if f.endswith(".mc")]
            self.file_dropdown.addItems(test_files)
        else:
            self.output_log.append("Warning: Base directory not found.")
    
    def compile_simulator(self):
        """Compile the C++ simulator"""
        self.output_log.clear()
        self.output_log.append("Compiling simulator...")
        self.progress_bar.show()
        
        try:
            # Custom compile command as specified by user
            cmd = ["g++", "-o", "simulator", "trueOrignal.cpp", "nonPipelined.cpp", "-std=c++11"]
            
            self.output_log.append(f"Running command: {' '.join(cmd)}")
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
            stdout, stderr = process.communicate()
            
            if process.returncode == 0:
                self.output_log.append("Compilation successful!")
                # Update executable path to match output name
                self.executable_path = os.path.join(self.base_dir, "simulator")
                if sys.platform == "win32":
                    self.executable_path += ".exe"
            else:
                self.output_log.append(f"Compilation failed: {stderr}")
                QMessageBox.critical(self, "Compilation Error", f"Failed to compile simulator:\n{stderr}")
        except Exception as e:
            self.output_log.append(f"Error during compilation: {str(e)}")
            QMessageBox.critical(self, "Compilation Error", f"An error occurred during compilation:\n{str(e)}")
        
        self.progress_bar.hide()
    
    def run_simulation(self):
        """Run the simulator with the selected test file"""
        # Check if executable exists
        if not os.path.exists(self.executable_path):
            reply = QMessageBox.question(self, "Compile First",
                                      "The simulator hasn't been compiled yet. Compile now?",
                                      QMessageBox.Yes | QMessageBox.No,
                                      QMessageBox.Yes)
            if reply == QMessageBox.Yes:
                self.compile_simulator()
            else:
                return

        if not os.path.exists(self.executable_path):
            QMessageBox.critical(self, "Error", "Could not find simulator executable.")
            return

        # Get selected test file
        if self.file_dropdown.count() == 0:
            QMessageBox.critical(self, "Error", "No test files available.")
            return

        selected_file = self.file_dropdown.currentText()
        input_path = os.path.join(self.base_dir, selected_file)

        if not os.path.exists(input_path):
            QMessageBox.critical(self, "Error", f"Test file not found: {input_path}")
            return

        # Clear previous results
        self.clear_results()
        self.output_log.append(f"Running simulation with input file: {selected_file}")
        
        # Clear any existing cycle_snapshots.log file to avoid confusion with old data
        snapshots_path = os.path.join(self.base_dir, "cycle_snapshots.log")
        try:
            if os.path.exists(snapshots_path):
                os.remove(snapshots_path)
                self.output_log.append("Removed old cycle_snapshots.log file")
        except Exception as e:
            self.output_log.append(f"Warning: Could not remove old log file: {str(e)}")
        
        self.progress_bar.show()
        
        # Get trace instruction parameters
        trace_inst_num = None
        trace_inst_pc = None
        
        if self.trace_inst_checkbox.isChecked():
            if self.trace_by_num_radio.isChecked() and self.trace_inst_num_input.text():
                try:
                    trace_inst_num = int(self.trace_inst_num_input.text())
                    self.output_log.append(f"Will trace instruction #{trace_inst_num}")
                except ValueError:
                    self.output_log.append(f"Warning: Invalid instruction number: {self.trace_inst_num_input.text()}")
            elif self.trace_by_pc_radio.isChecked() and self.trace_pc_input.text():
                try:
                    # Try to parse as hex or decimal
                    if self.trace_pc_input.text().startswith("0x"):
                        trace_inst_pc = int(self.trace_pc_input.text(), 16)
                    else:
                        trace_inst_pc = int(self.trace_pc_input.text())
                    self.output_log.append(f"Will trace instructions at PC {hex(trace_inst_pc)}")
                except ValueError:
                    self.output_log.append(f"Warning: Invalid PC value: {self.trace_pc_input.text()}")
            else:
                self.output_log.append("Warning: Trace is enabled but no value is specified")
        
        # Log the state of the pipeline print checkbox
        pipeline_print = self.print_pipeline_checkbox.isChecked()
        self.output_log.append(f"Print Pipeline Registers: {'Enabled' if pipeline_print else 'Disabled'}")
        
        # Run simulation in a separate thread
        self.sim_thread = SimulatorThread(
            self.executable_path, 
            input_path, 
            pipelining=self.pipelining_checkbox.isChecked(),
            forwarding=self.forwarding_checkbox.isChecked(),
            print_reg_each_cycle=self.print_reg_checkbox.isChecked(),
            print_pipeline_reg=pipeline_print,  # Explicitly capture this value
            print_bp_info=self.print_bp_checkbox.isChecked(),
            save_snapshots=self.save_snapshots_checkbox.isChecked(),
            trace_inst=self.trace_inst_checkbox.isChecked(),
            trace_inst_num=trace_inst_num,
            trace_inst_pc=trace_inst_pc,
            step_mode=self.step_mode_checkbox.isChecked(),
            single_step=False
        )
        self.sim_thread.progress.connect(self.update_simulation_progress)
        self.sim_thread.finished.connect(self.simulation_finished)
        self.sim_thread.start()
    
    def update_simulation_progress(self, output):
        """Update the output log with simulation progress"""
        self.output_log.append(output)
        # Auto-scroll to bottom
        scrollbar = self.output_log.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())
    
    def simulation_finished(self, success, message):
        """Handle simulation completion"""
        self.progress_bar.hide()
        self.output_log.append(message)
        
        if success:
            # Load the results
            self.load_results(self.base_dir)
            
            # Load snapshots if available
            if self.save_snapshots_checkbox.isChecked():
                self.load_snapshots()
                
            # Check for cycle_snapshots.log even if save_snapshots is not checked
            elif os.path.exists(os.path.join(self.base_dir, "cycle_snapshots.log")):
                self.output_log.append("Found cycle_snapshots.log, loading snapshots...")
                self.load_snapshots()
    
    def load_snapshots(self):
        """Load snapshots from cycle_snapshots.log if it exists"""
        snapshots_path = os.path.join(self.base_dir, "cycle_snapshots.log")
        self.output_log.append(f"Looking for snapshots file at: {snapshots_path}")
        
        if os.path.exists(snapshots_path):
            try:
                self.snapshots = []
                current_snapshot = None
                cycle_count = 0
                
                with open(snapshots_path, 'r') as f:
                    self.output_log.append("Reading snapshots file...")
                    lines = f.readlines()
                    self.output_log.append(f"Found {len(lines)} lines in the file")
                    
                    for line in lines:
                        line = line.strip()
                        
                        # Skip empty lines
                        if not line:
                            continue
                            
                        # Skip separator lines
                        if line.startswith("---"):
                            continue
                        
                        # Check if this is a cycle indicator line
                        if "Cycle" in line and "Pipeline State" in line:
                            try:
                                # Extract cycle number
                                parts = line.split()
                                cycle_num = int(parts[1])
                                
                                # Create new snapshot
                                current_snapshot = {
                                    "clockCycles": cycle_num,
                                    "if_id": {"valid": False},
                                    "id_ex": {"valid": False},
                                    "ex_mem": {"valid": False},
                                    "mem_wb": {"valid": False}
                                }
                                self.snapshots.append(current_snapshot)
                                cycle_count += 1
                            except Exception as e:
                                self.output_log.append(f"Error parsing cycle line: {line}")
                                self.output_log.append(f"Error: {str(e)}")
                            continue
                        
                        # Skip if no current snapshot
                        if current_snapshot is None:
                            continue
                            
                        # Parse IF stage
                        if line.startswith("IF:"):
                            try:
                                # Set valid to true since stage is present
                                current_snapshot["if_id"]["valid"] = True
                                
                                # Parse PC value
                                if "PC =" in line:
                                    pc_idx = line.find("PC =") + 5
                                    pc_end = line.find(",", pc_idx)
                                    if pc_end == -1:
                                        pc_end = len(line)
                                    pc_val = line[pc_idx:pc_end].strip()
                                    try:
                                        current_snapshot["if_id"]["pc"] = int(pc_val, 16)
                                    except ValueError:
                                        current_snapshot["if_id"]["pc"] = pc_val
                                
                                # Parse instruction
                                if "Instruction =" in line:
                                    inst_idx = line.find("Instruction =") + 13
                                    inst_end = len(line)
                                    inst_val = line[inst_idx:inst_end].strip()
                                    try:
                                        current_snapshot["if_id"]["instruction"] = int(inst_val, 16)
                                    except ValueError:
                                        current_snapshot["if_id"]["instruction"] = inst_val
                            except Exception as e:
                                self.output_log.append(f"Error parsing IF line: {line}")
                                self.output_log.append(f"Error: {str(e)}")
                        
                        # Parse ID stage
                        elif line.startswith("ID:"):
                            try:
                                # Set valid to true since stage is present
                                current_snapshot["id_ex"]["valid"] = True
                                
                                # Parse PC value
                                if "PC =" in line:
                                    pc_idx = line.find("PC =") + 5
                                    pc_end = line.find(",", pc_idx)
                                    if pc_end == -1:
                                        pc_end = len(line)
                                    pc_val = line[pc_idx:pc_end].strip()
                                    try:
                                        current_snapshot["id_ex"]["pc"] = int(pc_val, 16)
                                    except ValueError:
                                        current_snapshot["id_ex"]["pc"] = pc_val
                                
                                # Parse instruction type
                                if "Type =" in line:
                                    type_idx = line.find("Type =") + 7
                                    type_end = line.find(",", type_idx)
                                    if type_end == -1:
                                        type_end = len(line)
                                    current_snapshot["id_ex"]["instType"] = line[type_idx:type_end].strip()
                                
                                # Parse instruction subtype
                                if "Subtype =" in line:
                                    subtype_idx = line.find("Subtype =") + 10
                                    subtype_end = line.find(",", subtype_idx)
                                    if subtype_end == -1:
                                        subtype_end = len(line)
                                    current_snapshot["id_ex"]["subType"] = line[subtype_idx:subtype_end].strip()
                            except Exception as e:
                                self.output_log.append(f"Error parsing ID line: {line}")
                                self.output_log.append(f"Error: {str(e)}")
                        
                        # Parse EX stage
                        elif line.startswith("EX:"):
                            try:
                                # Set valid to true since stage is present
                                current_snapshot["ex_mem"]["valid"] = True
                                
                                # Parse PC value
                                if "PC =" in line:
                                    pc_idx = line.find("PC =") + 5
                                    pc_end = line.find(",", pc_idx)
                                    if pc_end == -1:
                                        pc_end = len(line)
                                    pc_val = line[pc_idx:pc_end].strip()
                                    try:
                                        current_snapshot["ex_mem"]["pc"] = int(pc_val, 16)
                                    except ValueError:
                                        current_snapshot["ex_mem"]["pc"] = pc_val
                                
                                # Parse ALU result
                                if "ALU Result =" in line:
                                    result_idx = line.find("ALU Result =") + 12
                                    result_end = len(line)
                                    result_val = line[result_idx:result_end].strip()
                                    try:
                                        current_snapshot["ex_mem"]["aluResult"] = int(result_val)
                                    except ValueError:
                                        current_snapshot["ex_mem"]["aluResult"] = result_val
                            except Exception as e:
                                self.output_log.append(f"Error parsing EX line: {line}")
                                self.output_log.append(f"Error: {str(e)}")
                        
                        # Parse MEM stage
                        elif line.startswith("MEM:"):
                            try:
                                # Set valid to true since stage is present
                                current_snapshot["mem_wb"]["valid"] = True
                                
                                # Parse PC value
                                if "PC =" in line:
                                    pc_idx = line.find("PC =") + 5
                                    pc_end = line.find(",", pc_idx)
                                    if pc_end == -1:
                                        pc_end = len(line)
                                    pc_val = line[pc_idx:pc_end].strip()
                                    try:
                                        current_snapshot["mem_wb"]["pc"] = int(pc_val, 16)
                                    except ValueError:
                                        current_snapshot["mem_wb"]["pc"] = pc_val
                                
                                # Parse type as memory data
                                if "Type =" in line:
                                    type_idx = line.find("Type =") + 7
                                    type_end = line.find(",", type_idx)
                                    if type_end == -1:
                                        type_end = len(line)
                                    current_snapshot["mem_wb"]["memData"] = line[type_idx:type_end].strip()
                            except Exception as e:
                                self.output_log.append(f"Error parsing MEM line: {line}")
                                self.output_log.append(f"Error: {str(e)}")
                                
                        # Parse WB stage
                        elif line.startswith("WB:"):
                            try:
                                # Extract PC value
                                if "PC =" in line:
                                    pc_idx = line.find("PC =") + 5
                                    pc_end = line.find(",", pc_idx)
                                    if pc_end == -1:
                                        pc_end = len(line)
                                    pc_val = line[pc_idx:pc_end].strip()
                                    try:
                                        pc = int(pc_val, 16)
                                        # Store in a WB field that the visualizer can access
                                        current_snapshot["wb_pc"] = pc
                                        current_snapshot["wb_valid"] = True
                                    except ValueError:
                                        pass
                                        
                                # Extract register write information
                                if "Writing to" in line:
                                    # Get destination register and value
                                    writing_idx = line.find("Writing to") + 11
                                    reg_idx = line.find("x", writing_idx)
                                    equals_idx = line.find("=", writing_idx)
                                    
                                    if equals_idx != -1 and reg_idx != -1 and reg_idx < equals_idx:
                                        value_str = line[equals_idx+1:].strip()
                                        try:
                                            # Store in a WB field that the visualizer can access
                                            current_snapshot["wb_result"] = int(value_str)
                                        except ValueError:
                                            current_snapshot["wb_result"] = value_str
                            except Exception as e:
                                self.output_log.append(f"Error parsing WB line: {line}")
                                self.output_log.append(f"Error: {str(e)}")
                
                self.output_log.append(f"Parsed {cycle_count} cycles, created {len(self.snapshots)} snapshots")
                
                if self.snapshots:
                    self.current_snapshot_index = 0
                    self.cycle_slider.setMinimum(0)
                    self.cycle_slider.setMaximum(len(self.snapshots) - 1)
                    self.cycle_slider.setValue(0)
                    self.cycle_slider.setEnabled(True)
                    self.prev_button.setEnabled(False)
                    self.next_button.setEnabled(len(self.snapshots) > 1)
                    
                    # Update display with the first snapshot
                    self.update_snapshot_display()
                    
                    self.output_log.append(f"Loaded {len(self.snapshots)} snapshots from cycle_snapshots.log.")
                else:
                    self.output_log.append("No snapshots were parsed from the file.")
            except Exception as e:
                self.output_log.append(f"Error loading snapshots: {str(e)}")
                import traceback
                self.output_log.append(traceback.format_exc())
        else:
            self.output_log.append("No cycle_snapshots.log file found.")
    
    def clear_results(self):
        """Clear all displayed results"""
        self.stats_table.setRowCount(0)
        self.register_table.setRowCount(0)
        self.data_memory_table.setRowCount(0)
        self.stack_memory_table.setRowCount(0)
        self.bp_table.setRowCount(0)
        self.snapshot_table.setRowCount(0)
        
        # Reset snapshot navigation
        self.snapshots = []
        self.current_snapshot_index = 0
        self.cycle_slider.setValue(0)
        self.cycle_slider.setEnabled(False)
        self.prev_button.setEnabled(False)
        self.next_button.setEnabled(False)
        self.cycle_label.setText("Cycle: 0/0")
        
        # Clear pipeline visualization
        self.pipeline_visualizer.setSnapshot(None)
        self.pipeline_visualizer.update()
    
    def create_stats_tab(self):
        """Create the statistics tab"""
        stats_tab = QWidget()
        stats_layout = QVBoxLayout(stats_tab)
        
        # Create statistics table
        self.stats_table = QTableWidget()
        self.stats_table.setColumnCount(2)
        self.stats_table.setHorizontalHeaderLabels(["Statistic", "Value"])
        self.stats_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.ResizeToContents)
        self.stats_table.horizontalHeader().setSectionResizeMode(1, QHeaderView.Stretch)
        self.stats_table.setStyleSheet(f"""
            QTableWidget {{
                background-color: {LIGHTEST_BG};
                color: {TEXT_COLOR};
                gridline-color: #555555;
                border: 1px solid #444444;
                border-radius: 8px;
            }}
            QTableWidget::item {{
                padding: 4px;
            }}
            QTableWidget::item:selected {{
                background-color: {ACCENT_COLOR};
                color: white;
            }}
        """)
        
        stats_layout.addWidget(self.stats_table)
        self.tabs.addTab(stats_tab, "Statistics")
    
    def create_registers_tab(self):
        """Create the registers tab"""
        registers_tab = QWidget()
        registers_layout = QVBoxLayout(registers_tab)
        
        self.register_table = QTableWidget()
        self.register_table.setColumnCount(3)
        self.register_table.setHorizontalHeaderLabels(["Register", "Decimal", "Hex"])
        self.register_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.register_table.setStyleSheet(f"""
            QTableWidget {{
                background-color: {LIGHTEST_BG};
                color: {TEXT_COLOR};
                gridline-color: #555555;
                border: 1px solid #444444;
                border-radius: 8px;
            }}
            QTableWidget::item {{
                padding: 4px;
            }}
            QTableWidget::item:selected {{
                background-color: {ACCENT_COLOR};
                color: white;
            }}
        """)
        
        registers_layout.addWidget(self.register_table)
        self.tabs.addTab(registers_tab, "Registers")
    
    def create_memory_tab(self):
        """Create the memory tab"""
        memory_tab = QWidget()
        memory_layout = QVBoxLayout(memory_tab)
        
        memory_tabs = QTabWidget()
        
        # Data memory tab
        data_memory_widget = QWidget()
        data_memory_layout = QVBoxLayout(data_memory_widget)
        
        self.data_memory_table = QTableWidget()
        self.data_memory_table.setColumnCount(3)
        self.data_memory_table.setHorizontalHeaderLabels(["Address", "Decimal", "Hex"])
        self.data_memory_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.data_memory_table.setStyleSheet(f"""
            QTableWidget {{
                background-color: {LIGHTEST_BG};
                color: {TEXT_COLOR};
                gridline-color: #555555;
                border: 1px solid #444444;
                border-radius: 8px;
            }}
            QTableWidget::item {{
                padding: 4px;
            }}
            QTableWidget::item:selected {{
                background-color: {ACCENT_COLOR};
                color: white;
            }}
        """)
        
        data_memory_layout.addWidget(self.data_memory_table)
        memory_tabs.addTab(data_memory_widget, "Data Memory")
        
        # Stack memory tab
        stack_memory_widget = QWidget()
        stack_memory_layout = QVBoxLayout(stack_memory_widget)
        
        self.stack_memory_table = QTableWidget()
        self.stack_memory_table.setColumnCount(3)
        self.stack_memory_table.setHorizontalHeaderLabels(["Address", "Decimal", "Hex"])
        self.stack_memory_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.stack_memory_table.setStyleSheet(f"""
            QTableWidget {{
                background-color: {LIGHTEST_BG};
                color: {TEXT_COLOR};
                gridline-color: #555555;
                border: 1px solid #444444;
                border-radius: 8px;
            }}
            QTableWidget::item {{
                padding: 4px;
            }}
            QTableWidget::item:selected {{
                background-color: {ACCENT_COLOR};
                color: white;
            }}
        """)
        
        stack_memory_layout.addWidget(self.stack_memory_table)
        memory_tabs.addTab(stack_memory_widget, "Stack Memory")
        
        memory_layout.addWidget(memory_tabs)
        self.tabs.addTab(memory_tab, "Memory")
    
    def create_bp_tab(self):
        """Create the branch predictor tab"""
        bp_tab = QWidget()
        bp_layout = QVBoxLayout(bp_tab)
        
        self.bp_table = QTableWidget()
        self.bp_table.setColumnCount(4)
        self.bp_table.setHorizontalHeaderLabels(["PC", "Prediction", "Actual", "Outcome"])
        self.bp_table.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.bp_table.setStyleSheet(f"""
            QTableWidget {{
                background-color: {LIGHTEST_BG};
                color: {TEXT_COLOR};
                gridline-color: #555555;
                border: 1px solid #444444;
                border-radius: 8px;
            }}
            QTableWidget::item {{
                padding: 4px;
            }}
            QTableWidget::item:selected {{
                background-color: {ACCENT_COLOR};
                color: white;
            }}
        """)
        
        bp_layout.addWidget(self.bp_table)
        self.tabs.addTab(bp_tab, "Branch Predictor")
    
    def load_results(self, directory):
        """Load simulation results from directory"""
        if not directory:
            return
        
        self.output_log.append(f"Loading results from: {directory}")
        
        # Load stats
        stats_path = os.path.join(directory, "stats.out")
        if os.path.exists(stats_path):
            self.load_stats(stats_path)
            self.output_log.append("Loaded statistics.")
        else:
            self.output_log.append("Warning: stats.out file not found.")
        
        # Load registers
        registers_path = os.path.join(directory, "register.mem")
        if os.path.exists(registers_path):
            self.load_registers(registers_path)
            self.output_log.append("Loaded register values.")
        else:
            self.output_log.append("Warning: register.mem file not found.")
        
        # Load memory
        data_memory_path = os.path.join(directory, "D_Memory.mem")
        if os.path.exists(data_memory_path):
            self.load_data_memory(data_memory_path)
            self.output_log.append("Loaded data memory contents.")
        else:
            self.output_log.append("Warning: D_Memory.mem file not found.")
            
        stack_memory_path = os.path.join(directory, "stack_mem.mem")
        if os.path.exists(stack_memory_path):
            self.load_stack_memory(stack_memory_path)
            self.output_log.append("Loaded stack memory contents.")
        else:
            self.output_log.append("Warning: stack_mem.mem file not found.")
        
        # Load branch predictor
        bp_path = os.path.join(directory, "BP_info.txt")
        if os.path.exists(bp_path):
            self.load_bp(bp_path)
            self.output_log.append("Loaded branch predictor information.")
        else:
            self.output_log.append("Warning: BP_info.txt file not found.")
    
    def load_stats(self, file_path):
        """Load simulation statistics from file"""
        try:
            with open(file_path, 'r') as f:
                lines = f.readlines()
                
            self.stats_data = {}
            for line in lines:
                parts = line.strip().split(':')
                if len(parts) >= 2:
                    key = parts[0].strip()
                    value = ':'.join(parts[1:]).strip()
                    self.stats_data[key] = value
            
            # Update the stats table
            self.stats_table.setRowCount(0)
            for key, value in self.stats_data.items():
                row = self.stats_table.rowCount()
                self.stats_table.insertRow(row)
                self.stats_table.setItem(row, 0, QTableWidgetItem(key))
                self.stats_table.setItem(row, 1, QTableWidgetItem(value))
                
                # Apply color highlighting for important metrics
                if "CPI" in key or "Cycles" in key or "Instructions" in key:
                    self.stats_table.item(row, 0).setBackground(QBrush(QColor(70, 70, 90)))
                    self.stats_table.item(row, 1).setBackground(QBrush(QColor(70, 70, 90)))
                
            return True
        except Exception as e:
            self.output_log.append(f"Error loading statistics: {str(e)}")
            return False
    
    def load_registers(self, file_path):
        """Load register values from register.mem file"""
        self.register_data = {}
        
        register_descriptions = {
            "x0": "zero",
            "x1": "ra (Return Address)",
            "x2": "sp (Stack Pointer)",
            "x3": "gp (Global Pointer)",
            "x4": "tp (Thread Pointer)",
            "x5": "t0 (Temporary)",
            "x6": "t1 (Temporary)",
            "x7": "t2 (Temporary)",
            "x8": "s0/fp (Saved/Frame Pointer)",
            "x9": "s1 (Saved Register)",
            "x10": "a0 (Argument/Return Value)",
            "x11": "a1 (Argument/Return Value)",
            "x12": "a2 (Argument)",
            "x13": "a3 (Argument)",
            "x14": "a4 (Argument)",
            "x15": "a5 (Argument)",
            "x16": "a6 (Argument)",
            "x17": "a7 (Argument)",
            "x18": "s2 (Saved Register)",
            "x19": "s3 (Saved Register)",
            "x20": "s4 (Saved Register)",
            "x21": "s5 (Saved Register)",
            "x22": "s6 (Saved Register)",
            "x23": "s7 (Saved Register)",
            "x24": "s8 (Saved Register)",
            "x25": "s9 (Saved Register)",
            "x26": "s10 (Saved Register)",
            "x27": "s11 (Saved Register)",
            "x28": "t3 (Temporary)",
            "x29": "t4 (Temporary)",
            "x30": "t5 (Temporary)",
            "x31": "t6 (Temporary)"
        }
        
        with open(file_path, 'r') as f:
            lines = f.readlines()
            for line in lines:
                parts = line.strip().split(" - ")
                if len(parts) == 2:
                    reg, value_str = parts
                    # Handle hex format if present
                    try:
                        if "0x" in value_str and "(" in value_str:
                            # Format like "0x0 (0)"
                            hex_part = value_str.split("(")[0].strip()
                            value = int(hex_part, 16)
                        else:
                            value = int(value_str, 0)  # Auto-detect base (0x for hex)
                        self.register_data[reg] = value
                    except ValueError:
                        self.output_log.append(f"Warning: Could not parse register value: {value_str}")
                        self.register_data[reg] = 0
        
        # Update table
        self.register_table.setRowCount(len(self.register_data))
        for row, (reg, value) in enumerate(self.register_data.items()):
            # Create a bold font for selected registers
            bold_font = QFont()
            bold_font.setBold(True)
            
            # Register name
            reg_item = QTableWidgetItem(reg)
            self.register_table.setItem(row, 0, reg_item)
            
            # Decimal value
            dec_item = QTableWidgetItem(f"{value}")
            dec_item.setTextAlignment(Qt.AlignRight | Qt.AlignVCenter)
            self.register_table.setItem(row, 1, dec_item)
            
            # Hex value
            hex_item = QTableWidgetItem(f"0x{value:08x}")
            hex_item.setTextAlignment(Qt.AlignRight | Qt.AlignVCenter)
            self.register_table.setItem(row, 2, hex_item)
            
            # Highlight result register (a0/x10) and make it bold
            if reg == "x10":
                for col in range(3):
                    item = self.register_table.item(row, col)
                    item.setBackground(QBrush(QColor(HIGHLIGHT_COLOR)))
                    item.setForeground(QBrush(QColor("white")))
                    item.setFont(bold_font)
            
            # Make other important registers bold (like sp, ra, etc.)
            if reg in ["x1", "x2", "x3", "x10", "x11"]:
                for col in range(3):
                    item = self.register_table.item(row, col)
                    item.setFont(bold_font)
    
    def load_data_memory(self, file_path):
        """Load data memory from D_Memory.mem file"""
        self.data_memory = {}
        with open(file_path, 'r') as f:
            lines = f.readlines()
            for line in lines:
                if line.startswith("Addr "):
                    parts = line.split(":")
                    if len(parts) >= 2:
                        addr = parts[0].replace("Addr ", "").strip()
                        value_str = parts[1].strip()
                        
                        # Handle different formats
                        try:
                            if "(" in value_str:
                                # Format: "0x00000000 (0)"
                                hex_part = value_str.split("(")[0].strip()
                                value = int(hex_part, 16)
                            else:
                                value = int(value_str, 16)
                            self.data_memory[addr] = value
                        except ValueError:
                            self.output_log.append(f"Warning: Could not parse memory value: {value_str}")
                            self.data_memory[addr] = 0
        
        # Bold font for highlighting
        bold_font = QFont()
        bold_font.setBold(True)
        
        # Update table
        self.data_memory_table.setRowCount(len(self.data_memory))
        for row, (addr, value) in enumerate(self.data_memory.items()):
            addr_item = QTableWidgetItem(addr)
            self.data_memory_table.setItem(row, 0, addr_item)
            
            # Decimal value
            dec_item = QTableWidgetItem(f"{value}")
            dec_item.setTextAlignment(Qt.AlignRight | Qt.AlignVCenter)
            self.data_memory_table.setItem(row, 1, dec_item)
            
            # Hex value
            hex_item = QTableWidgetItem(f"0x{value:08x}")
            hex_item.setTextAlignment(Qt.AlignRight | Qt.AlignVCenter)
            self.data_memory_table.setItem(row, 2, hex_item)
            
            # Highlight first memory location (input value) with purple
            if row == 0:
                for col in range(3):
                    item = self.data_memory_table.item(row, col)
                    item.setBackground(QBrush(QColor(HIGHLIGHT_COLOR)))
                    item.setForeground(QBrush(QColor("white")))
                    item.setFont(bold_font)
    
    def load_stack_memory(self, file_path):
        """Load stack memory from stack_mem.mem file"""
        self.stack_memory = {}
        with open(file_path, 'r') as f:
            lines = f.readlines()
            for line in lines:
                if line.startswith("Addr "):
                    parts = line.split(":")
                    if len(parts) >= 2:
                        addr = parts[0].replace("Addr ", "").strip()
                        value_str = parts[1].strip()
                        
                        # Handle different formats
                        try:
                            if "(" in value_str:
                                # Format: "0x00000000 (0)"
                                hex_part = value_str.split("(")[0].strip()
                                value = int(hex_part, 16)
                            else:
                                value = int(value_str, 16)
                            self.stack_memory[addr] = value
                        except ValueError:
                            self.output_log.append(f"Warning: Could not parse stack value: {value_str}")
                            self.stack_memory[addr] = 0
        
        # Bold font for highlighting
        bold_font = QFont()
        bold_font.setBold(True)
        
        # Update table
        self.stack_memory_table.setRowCount(len(self.stack_memory))
        for row, (addr, value) in enumerate(self.stack_memory.items()):
            addr_item = QTableWidgetItem(addr)
            self.stack_memory_table.setItem(row, 0, addr_item)
            
            # Decimal value
            dec_item = QTableWidgetItem(f"{value}")
            dec_item.setTextAlignment(Qt.AlignRight | Qt.AlignVCenter)
            self.stack_memory_table.setItem(row, 1, dec_item)
            
            # Hex value
            hex_item = QTableWidgetItem(f"0x{value:08x}")
            hex_item.setTextAlignment(Qt.AlignRight | Qt.AlignVCenter)
            self.stack_memory_table.setItem(row, 2, hex_item)
            
            # Highlight non-zero entries with purple
            if value != 0:
                for col in range(3):
                    item = self.stack_memory_table.item(row, col)
                    item.setBackground(QBrush(QColor(HIGHLIGHT_COLOR)))
                    item.setForeground(QBrush(QColor("white")))
                    item.setFont(bold_font)
                    
    def load_bp(self, file_path):
        """Load branch predictor info from BP_info.txt file"""
        self.bp_data = []
        with open(file_path, 'r') as f:
            lines = f.readlines()
            # Skip header lines
            for line in lines[2:]:
                parts = line.strip().split("\t")
                if len(parts) >= 4:  # Ensure we have at least 4 parts
                    # Take only the first 4 columns that match our table
                    self.bp_data.append(parts[:4])
        
        # Bold font for highlighting
        bold_font = QFont()
        bold_font.setBold(True)
        
        # Update table
        self.bp_table.setRowCount(len(self.bp_data))
        for row, data in enumerate(self.bp_data):
            for col, value in enumerate(data):
                item = QTableWidgetItem(value)
                self.bp_table.setItem(row, col, item)
                
                # Highlight active entries
                if col == 1 and value == "Yes":
                    # Make sure all cells are created before setting background
                    for c in range(4):  # Changed from 5 to 4 to match actual column count
                        # Create items if they don't exist
                        if self.bp_table.item(row, c) is None:
                            self.bp_table.setItem(row, c, QTableWidgetItem(""))
                        # Now set the background
                        cell_item = self.bp_table.item(row, c)
                        cell_item.setBackground(QBrush(QColor(HIGHLIGHT_COLOR)))
                        cell_item.setForeground(QBrush(QColor("white")))
                        cell_item.setFont(bold_font)
    
    def step_simulation(self):
        """Run a single step of the simulation"""
        # Check if executable exists
        if not os.path.exists(self.executable_path):
            reply = QMessageBox.question(self, "Compile First",
                                      "The simulator hasn't been compiled yet. Compile now?",
                                      QMessageBox.Yes | QMessageBox.No,
                                      QMessageBox.Yes)
            if reply == QMessageBox.Yes:
                self.compile_simulator()
            else:
                return

        if not os.path.exists(self.executable_path):
            QMessageBox.critical(self, "Error", "Could not find simulator executable.")
            return

        # Get selected test file
        if self.file_dropdown.count() == 0:
            QMessageBox.critical(self, "Error", "No test files available.")
            return

        selected_file = self.file_dropdown.currentText()
        input_path = os.path.join(self.base_dir, selected_file)

        if not os.path.exists(input_path):
            QMessageBox.critical(self, "Error", f"Test file not found: {input_path}")
            return
            
        # If this is our first step, clear previous results
        if not hasattr(self, 'current_step') or self.current_step == 0:
            self.clear_results()
            self.current_step = 0
            self.output_log.append(f"Starting step-by-step simulation with input file: {selected_file}")
            
            # Clear any existing cycle_snapshots.log file to avoid confusion with old data
            snapshots_path = os.path.join(self.base_dir, "cycle_snapshots.log")
            try:
                if os.path.exists(snapshots_path):
                    os.remove(snapshots_path)
                    self.output_log.append("Removed old cycle_snapshots.log file")
            except Exception as e:
                self.output_log.append(f"Warning: Could not remove old log file: {str(e)}")
        
        # Update step counter and status
        self.current_step += 1
        self.output_log.append(f"Executing step {self.current_step}...")
        self.progress_bar.show()
        
        # Get trace instruction parameters
        trace_inst_num = None
        trace_inst_pc = None
        
        if self.trace_inst_checkbox.isChecked():
            if self.trace_by_num_radio.isChecked() and self.trace_inst_num_input.text():
                try:
                    trace_inst_num = int(self.trace_inst_num_input.text())
                except ValueError:
                    self.output_log.append(f"Warning: Invalid instruction number: {self.trace_inst_num_input.text()}")
            elif self.trace_by_pc_radio.isChecked() and self.trace_pc_input.text():
                try:
                    # Try to parse as hex or decimal
                    if self.trace_pc_input.text().startswith("0x"):
                        trace_inst_pc = int(self.trace_pc_input.text(), 16)
                    else:
                        trace_inst_pc = int(self.trace_pc_input.text())
                except ValueError:
                    self.output_log.append(f"Warning: Invalid PC value: {self.trace_pc_input.text()}")
        
        # Log the state of the pipeline print checkbox
        pipeline_print = self.print_pipeline_checkbox.isChecked()
        self.output_log.append(f"Print Pipeline Registers: {'Enabled' if pipeline_print else 'Disabled'}")
        
        # Run simulation in a separate thread - with single_step flag
        self.sim_thread = SimulatorThread(
            self.executable_path, 
            input_path, 
            pipelining=self.pipelining_checkbox.isChecked(),
            forwarding=self.forwarding_checkbox.isChecked(),
            print_reg_each_cycle=self.print_reg_checkbox.isChecked(),
            print_pipeline_reg=pipeline_print,  # Explicitly capture this value
            print_bp_info=self.print_bp_checkbox.isChecked(),
            save_snapshots=True,  # Always save snapshots in step mode
            trace_inst=self.trace_inst_checkbox.isChecked(),
            trace_inst_num=trace_inst_num,
            trace_inst_pc=trace_inst_pc,
            step_mode=True,  # Always true for step simulation
            single_step=True
        )
        self.sim_thread.progress.connect(self.update_simulation_progress)
        self.sim_thread.finished.connect(self.step_finished)
        self.sim_thread.start()
    
    def step_finished(self, success, message):
        """Handle completion of a single step"""
        self.progress_bar.hide()
        
        if success:
            self.output_log.append(f"Step {self.current_step} completed")
            
            # Load the results
            self.load_results(self.base_dir)
            
            # Always try to load snapshots in step mode
            self.load_snapshots()
            
            # Update UI to show we're in step mode
            self.step_mode_checkbox.setChecked(True)
        else:
            self.output_log.append(f"Step failed: {message}")
            # Reset step counter on failure
            self.current_step = 0

if __name__ == "__main__":
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    window = SimulatorGUI()
    window.show()
    sys.exit(app.exec_()) 