'''
Command-line tool suite for Open Spectral Sensing (OSS) device.
'''

# TODO IPR Sync feature copies datapoints into a special file and only transfers new data
# TODO IPR look into high CPU usage while trasferring serial, slow serial
# TODO IPR fix sync issue when log file deleted manually
# TODO IPR scheduled capture start and stop
# TODO use SDFat instead of SD in Arduino to improve SD performance

import math
import serial
import serial.tools.list_ports
import time
import datetime
import os
from threading import Thread
from os.path import exists
import matplotlib.pyplot as plt

# helpers
cls = lambda: os.system('cls')              # clear console

# CONSTANTS
BAUDRATE = 921600                           # baudrate
SER_TIMEOUT = 10                            # serial timeout (should be a few seconds longer than device sleep time)
MIN_WAVELENGTH = 340                        # sensor minimum wavelength
MAX_WAVELENGTH = 1010                       # sensor maximum wavelength
WAVELENGTH_STEPSIZE = 5                     # sensor stepsize
MIN_LOGGING_INTERVAL = 10000                # the minimum logging interval
MAX_TRANSFER_DATAPOINTS = 100               # the suggested maximum number of datapoints to transfer over serial

# serial
s = None                                    # the currently selected serial device
command_to_send = ""                        # the serial instruction to send to device

# FIILE IO
SAVE_DIR = "./data/"                        # directory for saving files
FILE_EXT = ".CSV"                           # file extension
SYNC_FILE_NAME = "SYNC"

# user input
inp = ""

# Instructions that the OSS device understands
commands = {
    "TOGGLE_DATA_CAPTURE": "00",
    "MANUAL_CAPTURE": "01",
    "EXPORT_ALL": "02",
    "_SYNC_DATAPOINTS": "15",
    "ERASE_STORAGE": "14",
    "SET_COLLECTION_INTERVAL": "04",
    "SET_DEVICE_NAME": "08",
    "SET_CALIBRATION_FACTOR": "11",
    "RESET_DEVICE": "03",
    
    "_START_RECORDING": "12",
    "_STOP_RECORDING": "13",
    "_SET_DATETIME": "05",
    "_SAY_HELLO": "07",
    "_GET_INFO": "09",
    "_NSP_SETTINGS": "10",
    "_SET_START_TIME": "17",
    "_SET_STOP_TIME": "18",
}

local_functions = [
    "DISCONNECT",
    "CONFIGURE_SENSOR",
    # "TIMED_START_STOP",
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
def open_file(filename, add_header = False, open_mode = 'w', overwrite = False):
    
    # if the "data" directory doesn't exist, create it
    if (not os.path.exists(SAVE_DIR)): os.makedirs(SAVE_DIR)
    
    f = None
    # open a file to write response into
    file_suffix = -1
    while True:
        save_filename = SAVE_DIR + str(filename) + (str(file_suffix) if file_suffix > -1 else "") + FILE_EXT
        if exists(save_filename) and not overwrite:
            file_suffix += 1
        else:
            f = open(save_filename, open_mode)
            break
        
    # add the file header
    if (add_header): f.write(file_header())
    return f, save_filename

# Create a folder if not exists, otherwise create a new folder with unique numeral suffix
def open_folder(foldername):   
    folder_suffix = -1
    new_foldername = ""
    while True:
        new_foldername = foldername + (str(folder_suffix) if folder_suffix > -1 else "")
        if (os.path.exists(new_foldername)):
            folder_suffix += 1
        else:
            os.makedirs(new_foldername)
            break
    return new_foldername
        
    

# produce the file header
def file_header():
    line = "DATE,TIME,MANUAL,INT_TIME,FRAME_AVG,AE,QUALITY,X,Y,Z,"
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
        s.write(bytes(commands["_SAY_HELLO"] + '\n', 'utf-8'))
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
                      "logging_interval": int(logging_interval), 
                      "data_counter": int(data_counter),
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
    quality = int(int(tokens[6]))
    cie_x = float(tokens[7])
    cie_y = float(tokens[8])
    cie_z = float(tokens[9])
    
    x = [i for i in range(MIN_WAVELENGTH, MAX_WAVELENGTH + WAVELENGTH_STEPSIZE, WAVELENGTH_STEPSIZE)]
    y = [float(i) for i in tokens[10:-1]]

    return [x, y, timestamp, manual, int_time, frame_avg, ae, quality, cie_x, cie_y, cie_z]

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
        command_to_send = commands["_SET_DATETIME"] + date
        write_to_device(command_to_send, s)
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
            
            # filter hidden instructions
            public_commands = {}
            for (key, value) in commands.items():
                if (key[0] != '_'): public_commands[key] = value
            
            for i, key in enumerate(public_commands.keys()):
                if (key == "TOGGLE_DATA_CAPTURE"):
                    print("[" + str(i + 1) + "] " + ("STOP_RECORDING" if d["device_status"] == "1" else "START_RECORDING"))
                elif (key == "EXPORT_ALL"): 
                    print("[" + str(i + 1) + "] " + key + " (" + str(d["data_counter"]) + " entries)")
                else:
                    print("[" + str(i + 1) + "] " + key)
                    
            for i, key in enumerate(local_functions):
                print("[" + str(i + len(public_commands.keys()) + 1) + "] " + key)                
                
            inp = input("Choose a command by entering the number in front\n>")
            
            if (not inp.strip().isnumeric() or (int(inp) - 1) < 0 or (int(inp) - 1) >= (len(public_commands.keys()) +
                                                                            len(local_functions))):
                response = "Invalid entry. Please try again."
                continue
            
            inp = int(inp) - 1
            
            if (inp >= len(public_commands.keys())):
                # local function chosen
                inp = inp - len(public_commands.keys())
                selected_command = local_functions[inp]
                
                if (selected_command == "DISCONNECT"):
                    s.close()
                    cls()
                    print("Goodbye!")
                    exit()
                                         
                elif (selected_command == "CONFIGURE_SENSOR"):

                    device_name = ""
                    use_ae = True
                    calibration_factor = 1
                    frame_avg = 3
                    int_time = 500
                    collection_freq = 0
                    start_recording = False              

                    while True:
                        # get the device name
                        inp = input("Set a device name? (NSP)\n>").strip()
                        if inp.lower() == "cancel" or inp.lower() == "exit":
                            response = "Command cancelled. Datapoint not captured. Sensor not configured."
                            break
                        
                        if (len(inp) == 0):
                            device_name = ""
                        else:
                            device_name = inp
                            
                        cls()
                        # Use auto-exposure?
                        inp = input("Use auto-exposure? (y) or n\n>").strip()
                        if inp.lower() == "cancel" or inp.lower() == "exit":
                            response = "Command cancelled. Datapoint not captured. Sensor not configured."
                            break
                        
                        if (len(inp) == 0 or inp.lower() == 'y'):
                            use_ae = True
                            cancel = False
                            
                            # ask for custom integration time if auto-exposure not selected
                            if (not use_ae):
                                while True:                            
                                    inp = input("Enter a custom integration time between 1 and 1000 (500)\n>").strip()
                                    if inp.lower() == "cancel" or inp.lower() == "exit":
                                        response = "Command cancelled. Sensor not configured."
                                        cancel = True
                                        break
                                    
                                    if (len(inp) == 0):
                                        int_time = 500
                                    else:
                                        inp = int(inp)
                                        if (inp < 1 or inp > 1000):
                                            print("Enter a valid integration time between 1 and 1000")
                                            continue
                                        
                                        int_time = inp
                                        break
                                
                                if cancel: break
                            
                        else:
                            use_ae = False
                         
                        cls()
                        # set calibration factor?
                        inp = input("What is the calibration factor? (1.0)\n>").strip()
                        if inp.lower() == "cancel" or inp.lower() == "exit":
                            response = "Command cancelled. Sensor not configured.."
                            break
                        
                        if (len(inp) != 0 and is_float(inp)):
                            calibration_factor = inp
                        
                           
                        cls()
                        # how mnay frames to average into a single reading?
                        inp = input("How many frames to average? (3)\n>").strip()
                        if inp.lower() == "cancel" or inp.lower() == "exit":
                            response = "Command cancelled. Sensor not configured.."
                            break
                        
                        if (len(inp) == 0):
                            frame_avg = 3
                        else:
                            
                            if (not inp.isnumeric()):
                                print("Enter a valid integer.")
                                continue
                            
                            inp = int(inp)
       
                            if (inp < 1 or inp > 10):
                                print("Choose a number between [1, 10].")
                                continue
                            
                            frame_avg = inp
                        
                        cls()
                        # what is the collection frequency?
                        inp = input("What is the collection frequency in ms? (60000 or 1 minute)\n>").strip()
                        if inp.lower() == "cancel" or inp.lower() == "exit":
                            response = "Command cancelled. Sensor not configured."
                            break
                        
                        if (len(inp) == 0):
                            collection_freq = 60000
                        else:
                            
                            if (not inp.isnumeric()):
                                print("Enter a numeric value greater than " + str(MIN_LOGGING_INTERVAL))
                                continue
                            elif (int(inp) < MIN_LOGGING_INTERVAL):
                                print("Enter a value greater than " + str(MIN_LOGGING_INTERVAL))
                                continue
                            
                            collection_freq = int(inp)
                            
                        # start recording ?
                        inp = input("Start recording data? y or (n)\n>").strip()
                        if inp.lower() == "cancel" or inp.lower() == "exit":
                            response = "Command cancelled. Sensor not configured."
                            break
                        
                        if (len(inp) == 0 or inp.lower() == 'n'):
                            start_recording = False
                        else:
                            start_recording = True
                        
                        break
                        
                    # write name
                    write_to_device(commands["SET_DEVICE_NAME"] + "_" + device_name, s)
                    read_from_device(s)
                    
                    # write the settings
                    write_to_device(commands["_NSP_SETTINGS"] + str(int(use_ae)) + str(frame_avg).zfill(3) + str(int_time).zfill(4), s)
                    read_from_device(s)
                    
                    # set collection frequency
                    write_to_device(commands["SET_COLLECTION_INTERVAL"] + "_" + str(collection_freq), s)
                    read_from_device(s)
                    
                    # start recording or stop depending
                    if (start_recording):
                        write_to_device(commands["_START_RECORDING"], s)
                        read_from_device(s)
                    else:
                        write_to_device(commands["_STOP_RECORDING"], s)
                        read_from_device(s)
                        
                    response = "Device configured."
                    trigger_update = True
                    continue
                    
                elif (selected_command == "TIMED_START_STOP"):
                    response = "This feature has not been implemented yet."
                    continue
                       
                elif (selected_command == "REFRESH"):
                    trigger_update = True
                    response = "Refreshed."
                    continue
                
                else:
                    response = "UNKNOWN SYSTEM COMMAND"
                    continue

            # find the right command to send
            selected_command = list(public_commands.keys())[inp]
            command_to_send = ""
            
            if (selected_command == "TOGGLE_DATA_CAPTURE"):
                command_to_send = commands["TOGGLE_DATA_CAPTURE"]
                write_to_device(command_to_send, s)
                
                # no need to save the response
                read_from_device(s)    
                response = ""
                
                trigger_update = True
                
            elif (selected_command == "MANUAL_CAPTURE"):
                save_file = False
                user_filename = ""
                do_graph = False
                
                while True:
                    # ask if datapoint should be saved to file
                    inp = input("Do you want to save the result on this computer? y or n or cancel\n>").strip()
                    if inp.lower() == "cancel" or inp.lower() == "exit":
                        response = "Command cancelled. Datapoint not captured."
                        break
                    if (inp.lower() == 'y'):
                        save_file = True
                    elif (inp.lower() == 'n'):
                        save_file = False
                    else:
                        continue
                    
                    # ask for filename
                    if (save_file):
                        inp = input("Enter a file name or hit enter for automatic filename.\n>").strip()
                        if inp.lower() == "cancel" or inp.lower() == "exit":
                            response = "Command cancelled. Datapoint not captured."
                            break
                        if (len(inp) == 0):
                            user_filename = "MANUAL_" + get_formatted_date()
                        else:
                            user_filename = inp
                    
                    # ask if datapoint should be graphed
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
                        
                    command_to_send = commands["MANUAL_CAPTURE"]
                    write_to_device(command_to_send, s)
                    response = read_from_device(s)
                    
                    trigger_update = True
                    
                    if (save_file):
                        f, filename = open_file(user_filename)
                        f.write(file_header())
                        f.write(response[1] + "\n")
                        f.close()
                    
                    if (do_graph):
                        x, y, timestamp, manual, int_time, frame_avg, ae, quality, cie_x, cie_y, cie_z  = get_formatted_datapoint(response[1])
                        plt.plot(x,y)
                        plt.xlim([MIN_WAVELENGTH, MAX_WAVELENGTH])
                        plt.ylabel("Power (W/m^2)")
                        plt.xlabel("Wavelength (nm)")
                        plt.title("Manual Datapoint Capture at " + timestamp)
                        plt.text(500, 300, "X: " + str(cie_x), ha='center', va='center', transform=None)
                        plt.text(500, 280, "Y: " + str(cie_y), ha='center', va='center', transform=None)
                        plt.text(500, 260, "Z: " + str(cie_z), ha='center', va='center', transform=None)
                        plt.show()
                        
                    response = "Manual datapoint captured."
                    
                    break
                    
            elif (selected_command == "EXPORT_ALL"):
                
                # check if no data 
                if d["data_counter"] == 0:
                    response = "No data to export."
                    continue
                
                # check if too much data, 
                if (d["data_counter"] > MAX_TRANSFER_DATAPOINTS):
                    inp = input("For large amounts of data, it is recommended to read from the microSD directly. Do you wish to continue exporting anyways? (y) or n\n>").strip()
                    if (inp.lower() == 'n'):
                        response = "Export cancelled. No data transferred."
                        continue
                
                delete_data = False
                
                inp = input("Delete datapoints from device storage after exporting? (y) or n?\n>").strip()
                if inp.lower() == "cancel" or inp.lower() == "exit":
                    response = "Command cancelled. Data not exported."
                    continue
                
                if (len(inp) == 0 or inp.lower() == 'y'):
                    delete_data = True
                
                f, filename = open_file(get_formatted_date(), open_mode='wb')
                command_to_send = commands["EXPORT_ALL"]
                write_to_device(command_to_send, s)
                
                # read the "DATA" header
                h_data = s.readline().decode().strip()
                
                if (h_data.lower() != "data"):
                    response = "Could not export data. Please try again."
                    continue
                
                # read the file size header
                file_size = int(s.readline().decode().strip())
                
                if (file_size <= 0):
                    response = "Could not read file. Please try again."
                    continue

                bytes_read = 0
                
                while True:
                    if s.in_waiting > 0:
                                               
                        b = s.read(s.in_waiting)
                        bytes_read += len(b)
                        
                        percentage_transferred = (bytes_read / file_size) * 100
                        
                        cls()
                        print(str(round(percentage_transferred, 5)) + " % exported")
                    
                        if (b.strip()[-2:] == b'OK'):
                            break
                        
                        f.write(b)

                # close the file    
                f.close()
                response = "File saved as " + filename
                
                if delete_data:
                    trigger_update = True
                    write_to_device(commands["ERASE_STORAGE"], s)
                    res = read_from_device(s)
                    # if res[0].lower() != "OK":
                    #     response += " File could not be deleted from device storage."
                
            elif (selected_command == "SYNC_DATAPOINTS"):
                
                # open the sync file and find the file size in bytes
                sync_file_size = 0
                filename = ""
                f = None
                
                
                
                if not exists(SAVE_DIR + SYNC_FILE_NAME + FILE_EXT):
                    f, filename = open_file(SYNC_FILE_NAME, overwrite = True)
                else:
                    sync_file_size = os.stat(SAVE_DIR + SYNC_FILE_NAME + FILE_EXT).st_size
                    
                # close the datapoint file
                if f: f.close()
                
                # send the byte size to the sensor
                command_to_send = commands["SYNC_DATAPOINTS"]
                command_to_send += "_" + str(sync_file_size)
            
                print(command_to_send)
                exit()

                write_to_device(command_to_send, s)
                
                # read the DATA response
                response = s.readline().decode().strip()

                if (response.lower() != "data"):
                    response = "Could not SYNC."
                    continue
                
                # read the file size header
                file_size = int(s.readline().decode().strip())
                if (file_size == -1):
                    # sync mismatch
                    response = "There is a sync issue. Please export all sensor data and reset the device."
                    continue
            
                # open the sync file for writing
                f, filename = open_file("SYNC", open_mode = 'wb', overwrite = True)

                bytes_read = 0
                
                while True:
                    if s.in_waiting > 0:
                        
                        b = s.read(s.in_waiting)
                        bytes_read += len(b)
                        
                        cls()
                        print(str((bytes_read / file_size) * 100) + " % exported")
                        
                        if (b.strip()[-2:] == b'OK'):
                            break
                        
                        f.write(b)

                
                # close the SYNC file
                f.close()
                
                response = "Datapoints synchronized. " + str(bytes_read) + " bytes read."                

            elif (selected_command == "RESET_DEVICE"):
                command_to_send = commands["RESET_DEVICE"]
                write_to_device(command_to_send, s)
                response = read_from_device(s)
                if (response[0].lower() == "ok"):
                    response = "Device successfully reset to factory settings."
                else:
                    response = "Device could not be reset. Please try again."
                    
                trigger_update = True
                
            elif (selected_command == "ERASE_STORAGE"):
                command_to_send = commands["ERASE_STORAGE"]
                write_to_device(command_to_send, s)
                
                response = read_from_device(s)
                if (response[0].lower() == "ok"):
                    response = "Device storage successfully erased."
                else:
                    response = "Storage could not be erased. Please try again."
                
                trigger_update = True
                
            elif (selected_command == "SET_COLLECTION_INTERVAL"):
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
                    
                    command_to_send = commands["SET_COLLECTION_INTERVAL"] + "_" + inp.strip()
                    write_to_device(command_to_send, s)
                    response = read_from_device(s)

                    break
                trigger_update = True                    
                
            elif (selected_command == "SET_DEVICE_NAME"):
                while True:
                    inp = input("Enter the device name\n>").strip()
                    if (inp.lower() == "cancel" or inp.lower() == "exit"):
                        response = "Command cancelled. Name not set."
                        break   
                    
                    if (len(inp) > 12):
                        print("Name too long. Please try again.")
                        continue
                    
                    inp = inp.replace(' ', '_')
                    command_to_send = commands["SET_DEVICE_NAME"] + "_" + inp
                    write_to_device(command_to_send, s)
                    response = read_from_device(s)
                    
                    # update local variables
                    d["device_name"] = inp.strip()
                    
                    break
                
                trigger_update = True
                
            elif (selected_command == "_GET_INFO"):
                command_to_send = commands["GET_INFO"]
                write_to_device(command_to_send, s)
                response = read_from_device(s)
                
            elif (selected_command == "SET_CALIBRATION_FACTOR"):
                while True:
                    inp = input("Enter new sensor calibration factor\n>").strip()
                    if (inp.lower() == "cancel" or inp.lower() == "exit"):
                        response = "Command canelled. Calibration factor not set."
                        break   

                    if (not is_float(inp)):
                        print("Enter a numeric value")
                        continue
                    
                    command_to_send = commands["SET_CALIBRATION_FACTOR"] + "_" + inp
                    write_to_device(command_to_send, s)
                    response = read_from_device(s)
                    
                    break
                    

            else:
                response = "UNKNOWN COMMAND NOT SENT"
        

