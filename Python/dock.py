'''
Command-line tool suite for Open Spectral Sensing (OSS) device.
'''

import serial
import serial.tools.list_ports
import time
import datetime
import os
from threading import Thread
from os.path import exists
import matplotlib.pyplot as plt
import mplcursors as mpc

# helpers
cls = lambda: os.system('cls')              # clear console

# CONSTANTS
BAUDRATE = 921600                           # baudrate
SER_TIMEOUT = 10                            # serial timeout (should be a few seconds longer than device sleep time)
MIN_WAVELENGTH = 340                        # sensor minimum wavelength
MAX_WAVELENGTH = 1010                       # sensor maximum wavelength
WAVELENGTH_STEPSIZE = 5                     # sensor stepsize
MIN_LOGGING_INTERVAL = 10000                # the minimum logging interval

# serial
s = None                                    # the currently selected serial device
instruction = ""                            # the serial instruction to send to device

# FIILE IO
SAVE_DIR = "./data/"                        # directory for saving files
FILE_EXT = ".CSV"                           # file extension

# user input
inp = ""

# Instructions that the OSS device understands
instructions = {
    "TOGGLE_DATA_CAPTURE": "00",
    "MANUAL_CAPTURE": "01",
    "EXPORT_ALL": "02",
    "DELETE_ALL": "03",
    "SET_COLLECTION_INTERVAL": "04",
    "_SET_DATETIME": "05",
    "_SAY_HELLO": "07",
    "SET_DEVICE_NAME": "08",
    "GET_INFO": "09",
    "_NSP_SETTINGS": "10",
    "SET_CALIBRATION_FACTOR": "11",
}

local_functions = [
    "DISCONNECT",
    "SETUP",
    "REFRESH",
]

def connect_to_device(port_name):
    try:
        return serial.Serial(port=port_name, baudrate=BAUDRATE, timeout=SER_TIMEOUT)
    except Exception as e:
        return None

# write to all devices, or a specific one if id is supplied
def write_to_device(msg, serial_object):
    try:
        serial_object.write(bytes(msg + '\n', 'utf-8'))
        time.sleep(0.1)
        return True
    except Exception as e:
        return False
           
def read_from_device(serial_object):
    response = []
    line = ""
    while (line.lower() != "ok" and not ("err" in line.lower())):
        line = serial_object.readline().decode().strip()
        response.append(line)
    return response

# open a new file for writing. If file name already exists, append a number
def open_file(filename, add_header = False):
    f = None
    # open a file to write response into
    file_suffix = 0
    while True:
        save_filename = SAVE_DIR + str(filename) + str(file_suffix) + FILE_EXT
        if exists(save_filename):
            file_suffix += 1
        else:
            f = open(save_filename, 'w')
            break
        
    # add the file header
    if (add_header): f.write(file_header())
    return f, save_filename

# produce the file header
def file_header():
    line = "DATE,TIME,MANUAL,INT_TIME,FRAME_AVG,AE,IS_SATURATED,IS_DARK,X,Y,Z,"
    for i in range (MIN_WAVELENGTH, MAX_WAVELENGTH + WAVELENGTH_STEPSIZE, WAVELENGTH_STEPSIZE):
        line += str(i) + ","
    line += "\n"
    return line

# Pings all serial devices connected, saves responses of valid OSS devices. If response was valid, table contains name, else None
def say_hello(port, response, ind):
    try:
        with serial.Serial(port=port, baudrate=BAUDRATE, timeout=SER_TIMEOUT) as _s:
            device = update_device_status(_s)
            response[ind] = device
    except Exception as e:
        pass      
    
def update_device_status(s):
    try:
        s.write(bytes(instructions["_SAY_HELLO"] + '\n', 'utf-8'))
        time.sleep(0.1)
        
        res = s.readline().decode().strip()

        if res.lower() == "data":
            # if data is the first response, second response is the device name
            device_name = s.readline().decode().strip()
            logging_interval = s.readline().decode().strip()
            device_status = s.readline().decode().strip()
            data_counter = s.readline().decode().strip()
            # clear the buffer by reading the OK response
            _ok = s.readline()
            # save the name to the response
            device = {"port_name": s.port, 
                      "device_name": device_name, 
                      "logging_interval": logging_interval, 
                      "data_counter": data_counter,
                      "device_status": device_status}
    
        return device
    
    except Exception as e:
        return None
                
def find_devices():
    # list of serial ports connected to the computer
    ports = []
    
    while len(ports) <= 0:
        ports = serial.tools.list_ports.comports()        
        print("Looking for devices...")
        time.sleep(1)
        
    # for detecting which serial devices are open spectral sensors  
    responses = [None] * len(ports)
    threads = [None] * len(ports)
    
    # say hello to each device in a thread
    for i in range(len(threads)):
        threads[i] = Thread(target=say_hello, args=(ports[i].name, responses, i))
        threads[i].start()
    
    # wait for every device to respond
    for i in range(len(threads)): threads[i].join()
        
    # detected spectral sensors          
    devices = [device for device in responses if device is not None]

    return devices     

def get_formatted_date():
    return (str(datetime.datetime.now().year).zfill(4) +
            str(datetime.datetime.now().month).zfill(2) +
            str(datetime.datetime.now().day).zfill(2) +
            str(datetime.datetime.now().hour).zfill(2) +
            str(datetime.datetime.now().minute).zfill(2) +
            str(datetime.datetime.now().second).zfill(2))

# format a datapoint for quick graphing
def get_formatted_datapoint(line):
    tokens = line.split(',')
    
    timestamp = tokens[0] + " " + tokens[1]
    manual = bool(int(tokens[2]))
    int_time = int(tokens[3])
    frame_avg = int(tokens[4])
    ae = bool(int(tokens[5]))
    is_saturated = bool(int(tokens[6]))
    is_dark = bool(int(tokens[7]))
    cie_x = float(tokens[8])
    cie_y = float(tokens[9])
    cie_z = float(tokens[10])
    
    x = [i for i in range(MIN_WAVELENGTH, MAX_WAVELENGTH + WAVELENGTH_STEPSIZE, WAVELENGTH_STEPSIZE)]
    y = [float(i) for i in tokens[11:-1]]

    return [x, y, timestamp, manual, int_time, frame_avg, ae, is_saturated, is_dark, cie_x, cie_y, cie_z]

def flush_serial(s):
    while s.in_waiting > 0:
        s.readline()   
        
def is_float(val):     
    try:
        float(val)
        return True
    except ValueError:
        return False
    
    
if __name__ == "__main__":
              
    # start main loop
    while True:
        devices = find_devices()
        while True:
            # first loop, if only one device, automatically connect
            if (len(devices) == 1):
                inp = 0
            else:
                print("Detected devices:")
                
                for i, device in enumerate(devices):
                    print("[" + str(i) + "] " + device["device_name"])
                    
                inp = input("Select a device to connect to\n>")
                if (not inp.strip().isnumeric() or int(inp) < 0 or int(inp) >= len(devices)):
                    print("Invalid entry. Please try again.")
                    continue
                
                inp = int(inp)
            
            # S is the selected OSS device
            d = devices[inp]
            s = connect_to_device(d["port_name"])
            
            if (s is None):
                print("Could not connect, please try again.")
                continue
            
            break
        
        response = ""
        
        # device selected, set time
        date = get_formatted_date()
        instruction = instructions["_SET_DATETIME"] + date
        write_to_device(instruction, s)
        resp = read_from_device(s)
        
        if len(resp) <= 0 or resp[0].lower() != "ok":
            response = "Time could not be set."
            response = resp
        else:
            response = "Time has been set successfully to " + date
            
        
        flush_serial(s)
        
        # trigger device status update flag
        trigger_update = False
        
        while True:
            # update device status
            if (trigger_update):
                d = update_device_status(s)
                trigger_update = False
                
            cls()
            print(response)
            
            print('---')
            # second loop, choose instructions to send to the device selected
            print("Selected device: " + d["device_name"] + " on port: " + 
                  d["port_name"] + ".\nCurrently " +
                  ("RECORDING" if d["device_status"] == '1' else "PAUSED") +
                  ". Recording interval is set to " + str(d["logging_interval"]))
            print("---")
            
            
            
            for i, key in enumerate(instructions.keys()):
                if (key == "TOGGLE_DATA_CAPTURE"):
                    print("[" + str(i) + "] " + ("STOP_RECORDING" if d["device_status"] == "1" else "START_RECORDING"))
                elif (key == "EXPORT_ALL"): 
                    print("[" + str(i) + "] " + key + " (" + str(d["data_counter"]) + " entries)")
                else:
                    print("[" + str(i) + "] " + key)
                    
            for i, key in enumerate(local_functions):
                print("[" + str(i + len(instructions.keys())) + "] " + key)                
                
            inp = input("Choose a command by entering the number in front\n>")
            
            if (not inp.strip().isnumeric() or int(inp) < 0 or int(inp) >= (len(instructions.keys()) +
                                                                            len(local_functions))):
                response = "Invalid entry. Please try again."
                continue
            
            inp = int(inp)
            
            if (inp >= len(instructions.keys())):
                # local function chosen
                inp = inp - len(instructions.keys())
                cmd = local_functions[inp]
                
                if (cmd == "DISCONNECT"):
                    s.close()                        
                elif (cmd == "SETUP"):

                    device_name = ""


                    while True:
                        inp = input("Set a device name? (NSP)\n>").strip()
                        if (len(inp) == 0):
                            print("Invalid name")
                            continue
                            
                elif (cmd == "REFRESH"):
                    trigger_update = True
                    response = "Refreshed."
                    continue
                
                break

            # find the right command to send
            cmd = list(instructions.keys())[inp]
            instruction = ""
            
            if (cmd == "TOGGLE_DATA_CAPTURE"):
                instruction = instructions["TOGGLE_DATA_CAPTURE"]
                write_to_device(instruction, s)
                response = read_from_device(s)
                
                trigger_update = True
                
            elif (cmd == "MANUAL_CAPTURE"):
                save_file = False
                do_graph = False
                
                while True:
                    inp = input("Do you want to save the result? y or n or cancel\n>").strip()
                    if inp.lower() == "cancel" or inp.lower() == "exit":
                        response = "Command cancelled. Datapoint not captured."
                        break
                    if (inp.lower() == 'y'):
                        save_file = True
                    elif (inp.lower() == 'n'):
                        save_file = False
                    else:
                        continue
                    
                    inp = input("Do you want to graph the result? y or n or cancel\n>").strip()
                    if inp.lower() == "cancel" or inp.lower() == "exit":
                        response = "Command cancelled. Datapoint not captured."
                        break
                    
                    if (inp.lower() == 'y'):
                        do_graph = True
                    elif (inp.lower() == 'n'):
                        do_graph = False
                    else:
                        continue
                        
                    instruction = instructions["MANUAL_CAPTURE"]
                    write_to_device(instruction, s)
                    response = read_from_device(s)
                    
                    trigger_update = True
                    
                    if (save_file):
                        f, file_name = open_file("MANUAL_" + get_formatted_date())
                        f.write(file_header())
                        f.write(response[1] + "\n")
                        f.close()
                    
                    if (do_graph):
                        print(response)
                        x, y, timestamp, manual, int_time, frame_avg, ae, is_saturated, is_dark, cie_x, cie_y, cie_z  = get_formatted_datapoint(response[1])
                        plt.plot(x,y)
                        plt.xlim([MIN_WAVELENGTH, MAX_WAVELENGTH])
                        plt.ylabel("Power (W/m^2)")
                        plt.xlabel("Wavelength (nm)")
                        plt.title("Manual Datapoint Capture at " + timestamp)
                        plt.text(500, 300, "X: " + str(cie_x), ha='center', va='center', transform=None)
                        plt.text(500, 280, "Y: " + str(cie_y), ha='center', va='center', transform=None)
                        plt.text(500, 260, "Z: " + str(cie_z), ha='center', va='center', transform=None)
                        mpc.cursor(hover=True)
                        plt.show()
                        
                    break
                    
            elif (cmd == "EXPORT_ALL"):
                f, file_name = open_file(get_formatted_date())
                instruction = instructions["EXPORT_ALL"]
                write_to_device(instruction, s)

                while (True):
                    line = s.readline().decode().strip()
                    
                    if (line.lower() == "data"):
                        continue
                    
                    if (line.lower() == "ok"):
                        break
                    
                    # append the line to file
                    f.write(line + "\n")                
                # close the file    
                f.close()
                response = "File saved as " + file_name
                
            elif (cmd == "DELETE_ALL"):
                instruction = instructions["DELETE_ALL"]
                write_to_device(instruction, s)
                response = read_from_device(s)
                trigger_update = True
                
            elif (cmd == "SET_COLLECTION_INTERVAL"):
                while True:
                    inp = input("Enter the interval in ms\n>").strip()
                    if inp.lower() == "cancel" or inp.lower() == "exit":
                        response = "Command cancelled. Capture frequency unchanged."
                        break
                    
                    if (not inp.isnumeric()):
                        print("Enter a numeric value greater than " + str(MIN_LOGGING_INTERVAL))
                        continue
                    elif (int(inp) < MIN_LOGGING_INTERVAL):
                        print("Enter a value greater than " + str(MIN_LOGGING_INTERVAL))
                        continue
                    
                    instruction = instructions["SET_COLLECTION_INTERVAL"] + "_" + inp.strip()
                    write_to_device(instruction, s)
                    response = read_from_device(s)

                    break
                trigger_update = True                    
                
            elif (cmd == "SET_DEVICE_NAME"):
                while True:
                    inp = input("Enter the device name\n>").strip()
                    if (inp.lower() == "cancel" or inp.lower() == "exit"):
                        response = "Command cancelled. Name not set."
                        break   
                    
                    if (len(inp) > 12):
                        print("Name too long. Please try again.")
                        continue
                    
                    inp = inp.replace(' ', '_')
                    instruction = instructions["SET_DEVICE_NAME"] + "_" + inp
                    write_to_device(instruction, s)
                    response = read_from_device(s)
                    
                    # update local variables
                    d["device_name"] = inp.strip()
                    
                    break
                
                trigger_update = True
                
            elif (cmd == "GET_INFO"):
                instruction = instructions["GET_INFO"]
                write_to_device(instruction, s)
                response = read_from_device(s)
                
            elif (cmd == "SET_CALIBRATION_FACTOR"):
                while True:
                    inp = input("Enter new sensor calibration factor\n>").strip()
                    if (inp.lower() == "cancel" or inp.lower() == "exit"):
                        response = "Command canelled. Calibration factor not set."
                        break   

                    if (not is_float(inp)):
                        print("Enter a numeric value")
                        continue
                    
                    instruction = instructions["SET_CALIBRATION_FACTOR"] + "_" + inp
                    write_to_device(instruction, s)
                    response = read_from_device(s)
                    
                    break
                    

            else:
                instruction = instructions[cmd]
                response = "UNKNOWN COMMAND NOT SENT"
        

