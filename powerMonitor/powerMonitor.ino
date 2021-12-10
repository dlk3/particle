/*
 *  Copyright (C) 2021  David King <dave@daveking.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*  
 * Use a Particle Boron device as a remote power monitor.  Events are
 * published when the device loses/regains AC power.
 *
 * Create a powerMonitorConfig.h file containing:
 * - the email addresses for those who should receive event messages from
 *   this application,
 * - the name you gave the Particle webhook that handles the events published
 *   by the application,in the 
 * - the timezone offset value for your local timezone
 *  
 * See the powerMonitor.h.example file distributed with this project for
 * a template.
 * 
 * Set device names for your devices, based on their IDs, in the setup()
 * function below.
 * 
 **/

#include "powerMonitorConfig.h"
#include "math.h"

String location = "";
bool onUSB = false;
bool onBattery = false;
bool lowBattery = false;
time_t timePowerLost;
time_t lastMessageSent;

void setup() {

	// Set location string based on device id using arrays defined in powerMonitorConfig.h
	String id = System.deviceID();
	location = String::format("(unknown: %s)", id.c_str());
	for (int i=0; i < arraySize(deviceIDs); i++) {
		if (deviceIDs[i] == id) {
			location = deviceNames[i];
		}
	}

	// Set timezone
	setupTimeZone();
    
	//  For debugging, sending a message to the Particle events console ...
	//String s = Time.format(Time.now());
	//Particle.publish("logging", String::format("Current time is: %s", s.c_str()), PRIVATE);
	//delay(1000);

    //  Do the initial power source check 
    int powerSource = System.powerSource();
    if (powerSource == POWER_SOURCE_BATTERY) {
        onBattery = true;
        onUSB = false;
        timePowerLost = Time.now();
    }
    else {
        onBattery = false;
        onUSB = true;
    }
    
    //  Send an initial status message
    if (onBattery) {
        String s = String::format("%s Monitor Initialized - AC power is off, battery at %0.1f%% (%0.2fV)", location.c_str(), getBatteryPercentage(), getBatteryVolts());
        sendData(s);
    } else {
        String s = String::format("%s Monitor Initialized - AC power is on, battery at %0.1f%% (%0.2fV)", location.c_str(), getBatteryPercentage(), getBatteryVolts());
        sendData(s);        
    }
    
    //  Set up a function to allow me to query the power status
    //  from the Particle Console
    Particle.function("getStatus", getStatus);

	//  Set up a function that allows my network monitor to check the device
	//  via the Particle CLI ("particle call <device-name> areYouThere")
    Particle.function("areYouAlive", areYouAlive);
}

void loop() {
    String s;
    
    //  Wait 10 seconds
    delay(10000);
    
    //  Deal with DST changes
    setupTimeZone();
    
    //  Check if the power source has changed
    int powerSource = System.powerSource();
    if (powerSource == POWER_SOURCE_BATTERY) {
        if (!onBattery && onUSB) {
            //  Power source changed from USB to battery
            onBattery = true;
            onUSB = false;
            s = String::format("%s - AC power lost", location.c_str());
            sendData(s);
            timePowerLost = Time.now();
            lastMessageSent = Time.now();
        } else if (Time.now() - lastMessageSent > 3600) {
            //  If the power has been out for an hour, send another message
            s = String::format("%s - AC power has been out since %s", location.c_str(), Time.format(timePowerLost, "%r on %D").c_str());
            sendData(s);
            lastMessageSent = Time.now();
        }
    } else if (onBattery && !onUSB) {
        //  Power source changed from battery to USB
        onBattery = false;
        onUSB = true;
        s = String::format("%s - AC power restored", location.c_str());
        sendData(s);
    } 

    // Check the battery voltage
    float p = getBatteryPercentage();
    if (p <= 25) { 
        if (!lowBattery) {
            lowBattery = true;
            s = String::format("%s - Low battery - %0.1f%% (%0.2fV)", location.c_str(), p, getBatteryVolts());
            sendData(s);
        }
    } else {
        lowBattery = false;
    }
}

//  Publish an event to our webhook containing an e-mail address
//  and the message that should be sent to that address
void sendData(String message) {
    for (int i=0; i < arraySize(addresses); i++) {
        String s = String::format("[{\"key\":\"address\", \"value\":\"%s\"},{\"key\":\"message\", \"value\":\"%s\"}]", addresses[i].c_str(), message.c_str());
        Particle.publish(webhook, s, PRIVATE);
        delay(1000);
    };
}

//  Functions to return battery volts or percentage.  Percentage
//  formula for 3.7V LiPo batteries is by Roho in the thread at
//  https://electronics.stackexchange.com/questions/435837/calculate-battery-percentage-on-lipo-battery
float getBatteryVolts() {
    FuelGauge fuel;
    return fuel.getVCell();
}
float getBatteryPercentage() {
    return 123 - 123 / pow((1 + pow(getBatteryVolts() / 3.7, 80)), 0.165);
}

//  Query the current power status and publish a message containing the results
int getStatus(String command) {
    int powerSource = System.powerSource();
    String s = "";
    if (powerSource == POWER_SOURCE_BATTERY) {
        s = String::format("%s Status - AC power has been out since %s, battery at %0.1f%% (%0.2fV)", location.c_str(), Time.format(timePowerLost, "%r on %D").c_str(), getBatteryPercentage(), getBatteryVolts());
    } else {
        s = String::format("%s Status - AC power is on, battery at %0.1f%% (%0.2fV)", location.c_str(), getBatteryPercentage(), getBatteryVolts());
    }
    Particle.publish("getStatus", s, PRIVATE);
    delay(1000);
    sendData(s);
    return 1;
}

//  A simple test to see if the device is alive, for use by nagios
int areYouAlive(String command) {
	return 1;
}

//  Set the local timezone
void setupTimeZone() {
    waitFor(Time.isValid, 60000);
    int d = Time.month() * 100 + Time.day();
    if ((d > 314) and (d < 1107)) {
        Time.beginDST();
        Time.zone(timezoneOffset + 1);
    } else {
        Time.endDST();
        Time.zone(timezoneOffset);
    }
}
