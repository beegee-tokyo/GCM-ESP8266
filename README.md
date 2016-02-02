# GCM-ESP8266

## What does it do
Send notifications to Android devices with Google Cloud Messaging without additional web server<br />

## Why
I wanted to use Google Cloud Messaging on my ESP8266 to push notifications to Android devices. Most tutorials found on the web use a external webserver, PHP scripts and MySQL databases as an interface to send notifications via the Google Cloud Messaging (GCM) service. As the ESP8266 has a webserver capability, but no PHP or MySQL support I wrote this code to replace the external webserver, PHP scripts and MySQL database.<br />
![GCM](http://desire.giesecke.tk/wp-content/uploads/PushNoteServer-300x225.png)

## How
Instead of PHP scripts and a MySQL database I use below explained functions and SPIFFS filesystem for the GCM connection functions and saving the Android registration ids.<br />
![GCM](http://desire.giesecke.tk/wp-content/uploads/PushNoteESP-300x225.png)

## Required libraries
To send requests to the GCM servers JSON objects are used. The easiest way to implement JSON decoding and encoding is with the ArduinoJson library written by Benoit Blanchon.<br />
All credits for the ArduinoJson library go to Benoit Blanchon. You can get it from:<br />
[Blog on Good Code Smell](http://blog.benoitblanchon.fr/arduino-json-v5-0/)<br />
[Github sources](https://github.com/bblanchon/ArduinoJson)<br />
Install the library as explained in the ArduinoJson Wiki: [Using the library with Arduino](https://github.com/bblanchon/ArduinoJson/wiki/Using%20the%20library%20with%20Arduino)<br />
## Detailed description
[Blog](http://desire.giesecke.tk/esp8266-google-cloud-messaging-without-external-server/)<br />
[Wiki](https://github.com/beegee-tokyo/GCM-ESP8266/wiki)<br />

## Function references
###boolean gcmSendMsg()
#####Description
prepares the JSON object holding the registration IDs and data and calls gcmSendOut to forward the request to the GCM server
#####Signatures
    boolean gcmSendMsg(JsonArray& pushMessageIds, JsonArray& pushMessages);
    boolean gcmSendMsg(JsonObject& pushMessages);
#####Arguments
_pushMessageIds_ Json array with the key(s) for the message(s)<br />
_pushMessages_ Json array with the message(s) or Json object with key:message fields
#####Used global variables
Global string array *regAndroidIds[]* contains the ids<br />
Global int *regDevNum* contains number of devices
#####Return value
_true_ if the request was successful send to the GCM server<br />
_false_ if sending the request to the GCM server failed
#####Example
		// Create messages & keys as JSON arrays
		DynamicJsonBuffer jsonBufferKeys;
		DynamicJsonBuffer jsonBufferMsgs;
		JsonArray& msgKeysJSON = jsonBufferKeys.createArray();
		JsonArray& msgsJSON = jsonBufferMsgs.createArray();
		msgKeysJSON.add("sensortype");
		msgKeysJSON.add("value");
		msgsJSON.add("humitity");
		msgsJSON.add(95);
		gcmSendMsg(msgKeysJSON, msgsJSON);

###boolean writeRegIds()
#####Description
reads Android registration ids from the array regAndroidIds[] and saves them to the file gcm.txt as JSON object
#####Arguments
none
#####Used global variables
Global string array *regAndroidIds[]* contains the ids<br />
Global int *regDevNum* contains number of devices
#####Return value
_true_ if the registration ids were successfully saved<br />
_false_ if a file error occured

###boolean getRegisteredDevices()
#####Description
reads Android registration ids from the file gcm.txt and stores them in the array regAndroidIds[]
#####Arguments
none
#####Used global variables
Global string array *regAndroidIds[]* contains the ids<br />
Global int *regDevNum* contains number of devices
#####Return value
_true_ if the registration ids were successfully read<br />
_false_ if a file error occured or if the content of the file was corrupted
#####Example
    if (!getRegisteredDevice()) {
        Serial.println("Failed to read IDs");
    } else {
        if (regDevNum != 0) { // Any devices already registered?
            for (int i=0; i<regDevNum; i++) {
                Serial.println("Device #"+String(i)":");
                Serial.println(regAndroidIds[i]);
            }
        }
    }

###boolean addRegisteredDevice()
#####Description
adds a new Android registration id to the file gcm.txt
#####Arguments
_newDeviceID_ String containing the registration id
#####Used global variables
Global string array *regAndroidIds[]* contains the ids<br />
Global int *regDevNum* contains number of devices
#####Return value
_true_ if the registration id was successfully added<br />
_false_ if the registration id was invalid, if the max number of ids was reached or if a file error occured
#####Example
    String newID = "XXX91bFZIZMVJPeWjfEfaqMOWctyfAOifSl6Tz52BpCVHIsGmJnq7dr8XIAueSV2SsjkTTW_vlhDGOS8t-uuITk3jAe-d8NnYuuzhGdS3jGiXpgJYFAfz1gqndx_yz0zo3cWcLsJ0Usx";
    if (!addRegisteredDevice(newID)) {
        Serial.println("Failed to save ID");
    } else {
        Serial.println("Successful saved ID");
    }

###boolean delRegisteredDevice()
#####Description
deletes one or all Android registration id(s) from the file gcm.txt
#####Signatures
    boolean delRegisterDevice();
    boolean delRegisterDevice(String delRegId);
    boolean delRegisterDevice(int delRegIndex);
#####Arguments
_delRegId_ String with the registration id to be deleted<br />
_delRegIndex_ Index of the registration id to be deleted
#####Used global variables
Global string array *regAndroidIds[]* contains the ids<br />
Global int *regDevNum* contains number of devices
#####Return value
_true_ if the registration id(s) was/were successfully deleted<br />
_false_ if the registration id was not found, if the indwx was invalid or if a file error occured
#####Example
    // Delete registration id by the id itself
    String delID = "XXX91bFZIZMVJPeWjfEfaqMOWctyfAOifSl6Tz52BpCVHIsGmJnq7dr8XIAueSV2SsjkTTW_vlhDGOS8t-uuITk3jAe-d8NnYuuzhGdS3jGiXpgJYFAfz1gqndx_yz0zo3cWcLsJ0Usx";
    if (!delRegisteredDevice(delID)) {
        Serial.println("Failed to delete ID");
    } else {
        Serial.println("Successful deleted ID");
    }

    // Delete registration id at index
    int delIndex = 1;
    if (!delRegisteredDevice(delIndex)) {
        Serial.println("Failed to delete ID");
    } else {
        Serial.println("Successful deleted ID");
    }

    // Delete all saved registration ids
    if (!delRegisteredDevice()) {
        Serial.println("Failed to delete IDs");
    } else {
        Serial.println("Successful deleted all IDs");
    }

###boolean gcmSendOut()
#####Description
sends message to https://android.googleapis.com/gcm/send to	request a push notification to all registered Android devices
used internal only
#####Arguments
_data_ String with the Json object containing the reg IDs and data
#####Used global variables
_none_
#####Return value
_true_ if the request was successful send to the GCM server<br />
_false_ if sending the request to the GCM server failed
#####Example
		used internal only

