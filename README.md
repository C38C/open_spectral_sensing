# Open Spectral Sensing

## Table of Contents
1. [About the Project](#about-the-project)
2. [Overview](#overview)
3. [Getting Started](#getting-started)
4. [Build Guide](#build-guide)
7. [Calibration](#calibration)
8. [Troubleshooting](#troubleshooting)
9. [References](#references)
10. [Credits](#credits)

<h2 id="about-the-project">About the Project</h2>
Visible light is made up of several individual wavelengths, each with their own power level. By measuring the power distribution of light throughout the spectrum, we can begin to identify light and quantify its qualities. A spectral sensor measures the power levels at each wavelength (340 nm - 1010 nm). This data can be used for various use-cases such as in color calibration, agriculture, health, and more. 

<h2 id="overview">Overview</h2>
The sensor is programmed using Arduino (C++) and communicates via program written in Python. The sensor consists of off-the-shelf components from Sparkfun, and a spectral sensor from nanoLambda - producing an easy-to-assemble spectral sensor. The goal of this project is to make light research accessible to anyone.

<h2 id="getting-started">Getting Started</h2>

For wiring, and assembly instructions, see the [Build Guide](#build-guide).

<h3>Installation</h3>


- You must first install the firmware using provided [compiled binaries]() by following the instrucions below:
    
    - Connect the sensor to a computer via USB
    - with the sensor powered on, double tap the RESET button on the side. (If the sensor is enclosed in the case, you can use an M2 allen-key through the provided hole on the side of the case). This will bring up the "NRF52BOOT" sensor folder..
    - Drag and drop the "firmware_xxxx.uf2" file into this new folder. The sensor will automatically disconnect and update the firmware.

- (Optional) To build your own firmware using the provided source, you must install Arduino IDE and the following libraries:

    - Install Arduino IDE: https://www.arduino.cc/en/software
    - NSP32-Library: https://github.com/arsalan1322/NSP32-Library
    - Installing the Arduino Core for nRF52 Boards: https://learn.sparkfun.com/tutorials/nrf52840-development-with-arduino-and-circuitpython
    - Time library: https://github.com/PaulStoffregen/Time

<<<<<<< Updated upstream
- To connect to the sensor using ```dock.py```, first install matplotlib and pySerial.
```
$ pip install matplotlib
$ pip install pyserial
```

<h3>Usage</h3>

Once you have assembled the sensor, and flashed the firmware, you can connect to it using the ```dock.py``` program.
=======
    &nbsp;

    In Arduino IDE, set the following programming settings before uploading to the device:

    - Board: "SparkFun Pro nRF52840 Mini"
    - SoftDevice: "s140 6.1.1 r0"
    - Debug: Level 0 (Release)
    - Port: *choose the device port*
    - Programmer: "Bootloader DFU for Bluefruit nRF52"


<h3>Usage</h3>

Once you have assembled the sensor, flashed the firmware, and installed the required Python packages, you can connect to it using the ```dock.py``` program.
>>>>>>> Stashed changes
```
$ python dock.py
```
The program will begin looking for a connected spectral sensor, and automatically conenct to it. It will automatically synchronize the sensor time with the local computer time. The following menu will display:

```
Time has been set successfully to 20221005165331
---
Selected device: NSPS on port: COM20.
Currently PAUSED. Recording interval is set to 15000
---
[1] START_RECORDING
[2] MANUAL_CAPTURE
[3] EXPORT_ALL (0 entries)
[4] RESET_DEVICE
[5] SET_COLLECTION_INTERVAL
[6] SET_DEVICE_NAME
[7] SET_CALIBRATION_FACTOR
[8] DISCONNECT
[9] CONFIGURE_NSP
[10] REFRESH
Choose a command by entering the number in front
>
```
From here, you can enter ```9``` to enter ```CONFIGURE_NSP``` which will guide you through setting up the sensor for capturing data. 

From this menu, you can see the sensor name, sensor port, the current status of the sensor, as well as the current recording interval in ms.

You can also start a manual recording, or change other sensor settings.



Once the device has been detected by ```dock.py```, you would set the recording interval by entering ```5```, then start the recording by entering ```1```. Then, you can disconnect the device by entering ```8```.

At this point, you can disconnect the USB cable if the device is already connected to the battery. The device will continue to collect data according to the interval you set.

<i>Note that if the device powers off somehow due to a disconnected battery or lack of charge, it will stop recording until started again through</i> ```dock.py```.

<h3>Data</h3>

Data is stored on the sensor in a ".CSV" format. There are 146 columns. Columns 1 - 11 hold information about the spectral measurement, and columns 12 - 146 hold the spectral power distribution at 5 nm intervals from 340 nm to 1010 nm. Each row is a unique captured datapoint.

| Column # | Column title | Example data | Range    | Description                                                                                                       |
|----------|--------------|--------------|----------|-------------------------------------------------------------------------------------------------------------------|
| 1        | DATE         | 5/10/2022    | -        | The date on which this datapoint was captured. |
| 2        | TIME         | 12:30:37     | -        | The time on which this datapoint was captured. |
| 3        | MANUAL       | 1            | 0 or 1   | Was this datapoint captured manually? 1 if yes, 0 if no. |
| 4        | INT_TIME     | 448          | 1 - 1000 | The integration time used for this datapoint. Exposure time (ms) = (896*[IntegrationTime] + 160) / 500 |
| 5        | FRAME_AVG    | 3            | 1 - 10   | How many frames where averaged for this datapoint. |
| 6        | AE           | 1            | 0 or 1   | Was autoexposure enabled? 1 fi yes, 0 if no. |
| 7        | IS_SATURATED | 0            | 0 or 1   | Was this recording captured under too bright of a condition? 1 if yes, 0 if no. A saturated recording should not be used. |
| 8        | IS_DARK      | 0            | 0 or 1   | Was this recording captured under too dark of a condition? 1 if yes, 0 if no. A dark recording should not be used. |
| 9        | X            | 38.61        | -        | CIE1931 X value. |
| 10       | Y            | 37.52        | -        | CIE1931 Y value. |
| 11       | Z            | 21.73        | -        | CIE1931 Z value. |
| 12 - 146       | 340 - 1010            |  -       | -        | spectral power in W/m<sup>2</sup>. |

<h3>LEDs</h3>

The sensor has three on-board LEDs: red, blue, and orange. The red LED is always on when the sensor is powered on. The orange LED is only on when the battery is charging, off when charge complete or battery is not connected. The blue LED behaviour can be classified using the following list:

- Solid on when sensor has not been setup and it is not recording.
- On for a brief moment while sensor is collecting data, off otherwise.
- Flashes to indicate a problem. See [Troubleshooting](#troubleshooting).

<<<<<<< Updated upstream
=======
<h2 id="capturing-data">Capturing Data</h2>

There are three ways to capture data using the sensor.

1. <h3>Automatic</h3>

    Automatic capture is when you plug in the device, and start recording via the ```START_RECORDING``` menu option in ```dock.py```. The recording interval must be set. Recording will last until stopped via the dock program, or if power is lost.

2. <h3>Automatic timed start and stop</h3>

    This mode is for creating a start and end time for datapoint capture. You can schedule a recording to start, and/or end. 

3. <h3>Manual capture</h3>

    With the device plugged in and connected to the dock program, you can initiate a datapoint capture by selecting ```[2] MANUAL_CAPTURE```. A recording interval does not need to be set for this mode. The device will capture one measurement and save it to device memory.

With this feature, you can also preview a graph of the datapoint once captured. 

>>>>>>> Stashed changes
<h2 id="build-guide">Build Guide</h2>

<h3>Parts List</h3>

Electronics

1. [nanoLambda NSP32m W1](https://nanolambda.myshopify.com/products/nsp32m_w1_temp)
2. [SparkFun Pro nRF52840 Mini](https://www.sparkfun.com/products/15025)
3. [SparkFun microSD Transflash Breakout](https://www.sparkfun.com/products/544)
4. 2GB+ microSD card
5. [800mAh Li-Po battery with 2mm JST connector](https://www.solarbotics.com/product/17804)
6. [30 AWG Silicone Wire](https://www.amazon.ca/gp/product/B01M70EDCW/ref=ppx_yo_dt_b_asin_title_o04_s00?ie=UTF8&psc=1)

Hardware (optional)

<<<<<<< Updated upstream
6. [3D printed case]()
7. 2x M2 bolts and 2x nuts
=======
1. [3D printed case ]() (Found in ```3D Models\Case```)
2. 2x M2 bolts and 2x nuts (for cap)
>>>>>>> Stashed changes

<h3 id="layout">Layout</h3>

<img src="./Diagrams/wiring_diagram.png">

<img src="./Diagrams/layout_animation.gif">

<h3>Soldering</h3>

To allow for a small device footprint, flexible 30 AWG silicone wire is used. Approximately x cm of wire is used for the configuration outlined in [Layout](#layout)

Soldering happens from both the top and bottom of the nRF52840. "Helping hands" are highly recommended, as well as a fine-tip soldering iron.

<<<<<<< Updated upstream
<h3>Assembly</h3>
=======
The fully soldered sensor looks like this:

<img src="">

Use the following table to help cut, match, and solder wires:

| Wire  # | Length (cm) | Pin on nRF52840 | Pin on microSD card reader | Pin on nanoLambda NSP32m |
|---------|-------------|-----------------|----------------------------|--------------------------|
| 1       |             | GND             | GND                        | GND                      |
| 2       |             | 3V3             | VCC                        | 3V3                      |
| 3       |             | 4               | CS                         | -                        |
| 4       |             | 5               | -                          | CS                       |
| 5       |             | 28              | -                          | RST                      |
| 6       |             | 29              | -                          | RDY                      |
| 7       |             | 30              | SCK                        | SCK                      |
| 8       |             | 31              | DO/MISO                    | DO/MISO                  |
| 9       |             | 3               | DI/MOSI                    | DI/MOSI                  |

Wires for the microSD card reader are soldered from the bottom, wires for the NSP32m are soldered from the top.
Two wires connect to single common pins on the nRF52840 for GND, 3V3, SCK, DO, and DI.

<h3>3D Printing</h3>

The case consists of three separate parts that come together to enclose the sensor. They are locatetd in the ```3D Models\Case``` directory.


- <h4>Material</h4>

    PETG (recommended), PLA, ABS, etc. Recommended material is PETG due to its flexibiliy. PLA and ABS can be used, but repeated strain may cause parts to snap.

- <h4>Layer height</h4>

    0.12 mm or finer.

- <h4>Supports</h4>

    Yes, as shown in orange.

- <h4>Part orientation</h4>

    **If using FDM printer, the part orientation is important for maximum strentgh.**

    <img src="./Diagrams/print_orientation.png">
>>>>>>> Stashed changes






<h2 id="calibration">Calibration</h2>

...

<h2 id="troubleshooting">Troubleshooting</h2>

<h3>Blue LED flashes once: Problem with SD card.</h3>

1. Check if SD card is inserted
2. Plug in SD card to computer and delete all files stored on it

<h3>Blue LED flashes twice: Problem with internal memory</h3>

1. Connect the sensor to ```dock.py```
2. Select ```RESET_DEVICE``` by entering ```4```

<i>If problems persist, </i>

<h2 id="references">References</h2>

1. nanoLambda documentation
2. 

<h2 id="credits">Credits</h2>

...
