# RISC-V Processor Simulator GUI

A graphical simulator for visualizing and understanding the execution of RISC-V instructions in a pipelined processor architecture.

## Overview

This application provides a user-friendly interface for simulating the execution of RISC-V code, allowing students and researchers to understand the inner workings of a processor pipeline. The simulator visualizes the pipeline stages, memory states, register values, and performance statistics.

## Features

- **Pipelined Execution Visualization**: See the state of each pipeline stage (IF, ID, EX, MEM, WB) in real time
- **Step-by-step Execution**: Run the simulation step by step to analyze each cycle in detail
- **Data Forwarding**: Toggle data forwarding to understand its impact on performance
- **Pipeline Hazard Visualization**: View stalls and bubbles in the pipeline
- **Memory & Register Inspection**: Examine the contents of registers and memory during execution
- **Cycle-by-cycle Snapshots**: Save and review the state of the processor at each cycle
- **Branch Prediction Analysis**: Track branch predictor performance
- **Performance Statistics**: View CPI, instruction counts, and other performance metrics
- **Customizable Options**: Configure simulation parameters through a simple GUI

## Requirements

- Python 3.6+
- PyQt5
- C++ compiler (g++ recommended)
- RISC-V machine code files (.mc format)

## Installation

1. Clone this repository:
   ```
   git clone [repository-url]
   cd [repository-directory]
   ```

2. Install Python dependencies:
   ```
   pip install PyQt5
   ```

3. Compile the simulator:
   ```
   g++ -o simulator trueOrignal.cpp nonPipelined.cpp -std=c++11
   ```
   Alternatively, you can use the "Compile Simulator" button in the GUI.

## Usage

1. Run the GUI application:
   ```
   python GUI.py
   ```

2. Select a RISC-V machine code file (.mc) from the dropdown menu
   
3. Configure simulation options:
   - Toggle pipelining
   - Enable/disable data forwarding
   - Configure register and pipeline register printing
   - Set up instruction tracing
   - Enable step mode for cycle-by-cycle execution

4. Run the simulation by clicking "Run Simulation" or step through it with "Step"

5. View results in the various tabs:
   - Statistics: Performance metrics
   - Registers: Current register values
   - Memory: Data and stack memory values
   - Branch Predictor: Branch prediction statistics
   - Pipeline Snapshots: Detailed view of pipeline state

## Pipeline Visualization

The pipeline visualization shows the state of each pipeline stage (IF, ID, EX, MEM, WB) in each cycle, including:
- PC values
- Instruction details
- Stalls and bubbles
- Data forwarding paths
- Register values

## Step-by-Step Simulation

In step mode, you can:
1. Execute one cycle at a time with the "Step" button
2. View the complete state of the processor after each step
3. Navigate through saved snapshots using "Previous Cycle" and "Next Cycle" buttons
4. Use the cycle slider to jump to a specific cycle

## Input Files

The simulator accepts RISC-V machine code files in .mc format. These files should be placed in the same directory as the simulator.

## Acknowledgments

This project was developed as part of the Computer Architecture course curriculum. It is designed to help students understand the concepts of pipelined processor architecture and RISC-V instruction execution. 