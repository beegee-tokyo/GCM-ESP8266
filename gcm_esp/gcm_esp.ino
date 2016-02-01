/**
	Google Cloud Messaging tutorial
	part 1 
	-> registration id save/read/delete functions
	-> send request push notification to google cloud messaging server

	Hardware
	Adafruit HUZZAH ESP8266 (ESP-12) module - https://www.adafruit.com/products/2471

	Implements server app to register Android devices to GCM service.
	Stores Android GCM ids in the file system.
	Implements function to send a GCM push message to all registered Android devices

	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	Requires ArduinoJson library from Benoit Blanchon
	http://blog.benoitblanchon.fr/arduino-json-v5-0/
	https://github.com/bblanchon/ArduinoJson
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	To access the web server from outside your local LAN you need
	to setup a url and prepare your router/modem to make this IP
	accessible from the Internet
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	@author Bernd Giesecke
	@version 0.1 alpha January, 2016.
*/

/* Includes from libraties */
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <FS.h>

/* gcmInfo.h contains Google API key */
/* REMARK: For privacy reasons I do not share my settings.
	file content looks like: */
/* ===FILE CONTENT BELOW=========================================== */
//	/** Google API key */
//	const String API_key = "===YOUR_GOOGLE_API_KEY_HERE===";
/* ===FILE CONTENT ABOVE=========================================== */
#include "gcmInfo.h"

/* Flag for Serial output */
#define DEBUG_OUT
	
/** Google Cloud Messaging server address */
char serverName[] = "http://android.googleapis.com";
/** Google Cloud Messaging host */
String hostName = "android.googleapis.com";
/** WiFiClient class to communicate with GCM server */
WiFiClient gcmClient;
/** Reason of failure of received command */
String failReason = "unknown";

/** Array to hold the registered Android device IDs (max 5 devices, extend if needed)*/
#define MAX_DEVICE_NUM 5
String regAndroidIds[MAX_DEVICE_NUM];
/** Number of registered devices */
byte regDevNum = 0;

/* ===CODE_STARTS_HERE========================================== */
/**
	writeRegIds
	writes regAndroidIds[] to file gcm.txt as JSON object
	returns true if successful, false if file error
*/
boolean writeRegIds() {
	#ifdef DEBUG_OUT 
	Serial.println("===========================================");
	Serial.println("writeRegIds");
	Serial.println("===========================================");
	#endif
	// First check if the file system is mounted and working
	FSInfo fs_info;
	if (!SPIFFS.info(fs_info)) {
		// Problem with the file system
		#ifdef DEBUG_OUT 
		Serial.println("File system error");
		#endif
		failReason = "File system error";
		return false;
	}
	// Open config file for writing.
	File devicesFile = SPIFFS.open("/gcm.txt", "w");
	if (!devicesFile) {
		#ifdef DEBUG_OUT 
		Serial.println("Failed to open gcm.txt for writing");
		#endif
		failReason = "Failed to open gcm.txt for writing";
		return false;
	}
	// Create new device ids as JSON
	DynamicJsonBuffer jsonBuffer;

	// Prepare JSON object
	JsonObject& regIdJSON = jsonBuffer.createObject();

	// Add existing devices to JSON object (if any)
	for (int i=0; i<regDevNum; i++) {
		regIdJSON[String(i)] = regAndroidIds[i];
	}
	
	// Convert JSON object to String
	String jsonTxt;
	regIdJSON.printTo(jsonTxt);

	#ifdef DEBUG_OUT 
	Serial.println("New list of registered IDs: " + jsonTxt);
	#endif
	
	// Save status to file
	devicesFile.println(jsonTxt);
	devicesFile.close();
	return true;
}
/**
	getRegisteredDevices
	reads registered device IDs from file gcm.txt
	saves found devices in global variable regAndroidIds[]
	sets global variable regDevNum to number of devices found
	returns true if successful, false if file error or JSON parse error occured
*/
boolean getRegisteredDevices() {
	#ifdef DEBUG_OUT 
	Serial.println("===========================================");
	Serial.println("getRegisteredDevices");
	Serial.println("===========================================");
	#endif
	// First check if the file system is mounted and working
	FSInfo fs_info;
	if (!SPIFFS.info(fs_info)) {
		// Problem with the file system
		#ifdef DEBUG_OUT 
		Serial.println("File system error");
		#endif
		failReason = "File system error";
		return false;
	}
	// Then get registered devices from the files
	// open file for reading.
	File devicesFile = SPIFFS.open("/gcm.txt", "r");
	if (devicesFile) // Found file
	{
		// Read content from config file.
		String content = devicesFile.readString();
		devicesFile.close();
	
		// Convert file content into JSON object
		DynamicJsonBuffer jsonBuffer;
		JsonObject& regIdJSON = jsonBuffer.parseObject(content);
		if (regIdJSON.success()) // Found some entries
		{
			regDevNum = 0;
			for (int i=0; i<MAX_DEVICE_NUM; i++) {
				if (regIdJSON.containsKey(String(i))) { // Found an entry
					regAndroidIds[i] = regIdJSON[String(i)].asString();
					regDevNum++;
					#ifdef DEBUG_OUT 
					Serial.println("Found: " + regAndroidIds[i]);
					#endif
				}
			}
			#ifdef DEBUG_OUT 
			Serial.println("Found: " + String(regDevNum) + " devices");
			#endif
			return true;
		} else { // File content is not a JSON string
			#ifdef DEBUG_OUT 
			Serial.println("File content is not a JSON string");
			#endif
			failReason = "File content is not a JSON string";
			return false;
		}
	} else { // File not found -  not an error, just no registered IDs yet
		regDevNum = 0;
		#ifdef DEBUG_OUT 
		Serial.println("File does not exist");
		#endif
		failReason = "File does not exist";
		return true;
	}
}

/**
	addRegisteredDevice
	adds a new device to the file gcm.txt
	input String new device ID
	returns true if successful, false if a file error occured or ID is invalid
*/
boolean addRegisteredDevice(String newDeviceID) {
	#ifdef DEBUG_OUT 
	Serial.println("===========================================");
	Serial.println("addRegisteredDevice");
	Serial.println("===========================================");
	#endif
	// Basic check if newDeviceID is valid
	if (newDeviceID.length() != 140) {
		#ifdef DEBUG_OUT 
		Serial.println("Invalid device id");
		#endif
		failReason = "Invalid device id";
		return false;
	}
	// First get registered devices from the files
	if (!getRegisteredDevices) {
		#ifdef DEBUG_OUT 
		Serial.println("Error reading registered devices");
		#endif
		return false;
	}

	// Number of devices < MAX_DEVICE_NUM ???
	if (regDevNum == MAX_DEVICE_NUM) {
		#ifdef DEBUG_OUT 
		Serial.println("Maximum number of devices registered");
		Serial.println("Delete another device before adding new one");
		#endif
		failReason = "Maximum number of devices registered";
		return false;
	}

	// Check if id is already in the list
	for (int i=0; i<regDevNum; i++) {
		if (regAndroidIds[i] == newDeviceID) {
			#ifdef DEBUG_OUT 
			Serial.println("Devices already registered");
			#endif
			failReason = "Devices already registered";
			return false;
		}
	}

	// Add new ID to regAndroidIds[]
	regDevNum += 1;
	regAndroidIds[regDevNum-1] = newDeviceID;
	
	if (writeRegIds()) {
		#ifdef DEBUG_OUT 
		Serial.println("Successful added id: " + newDeviceID);
		#endif
		// Refresh list with registered devices
		getRegisteredDevices();
		return true;
	} else {
		#ifdef DEBUG_OUT 
		Serial.println("Failed to added id: " + newDeviceID);
		#endif
		// Refresh list with registered devices
		getRegisteredDevices();
		return false;
	}
}

/**
	delRegisteredDevice
	deletes all registration ida from the list
	returns true if successful, false if a file error occured
*/
boolean delRegisteredDevice() {
	#ifdef DEBUG_OUT Serial.println("===========================================");
	Serial.println("delRegisteredDevice");
	Serial.println("===========================================");
	#endif

	// First get registered devices from the files
	if (!getRegisteredDevices) {
		#ifdef DEBUG_OUT 
		Serial.println("Error reading registered devices");
		#endif
		return false;
	}

	#ifdef DEBUG_OUT 
	Serial.println("Trying to delete all ids");
	#endif
	if (SPIFFS.remove("/gcm.txt")) {
		#ifdef DEBUG_OUT 
		Serial.println("All registered device ids deleted");
		#endif
		regDevNum = 0;
		return true;
	} else {
		#ifdef DEBUG_OUT 
		Serial.println("Error while deleting registration file");
		#endif
		failReason = "Error while deleting registration file";
		return false;
	}
	return false;
}

/**
	delRegisteredDevice
	deletes a registration id from the list
	delRegId => registration id to be deleted. delAll must be false & delRegIndex must be 9999!!!!
	returns true if successful, false if a file error occured or ID is invalid or not registered 
*/
boolean delRegisteredDevice(String delRegId) {
	int delRegIndex;
	#ifdef DEBUG_OUT Serial.println("===========================================");
	Serial.println("delRegisteredDevice");
	Serial.println("===========================================");
	#endif

	// First get registered devices from the files
	if (!getRegisteredDevices) {
		#ifdef DEBUG_OUT 
		Serial.println("Error reading registered devices");
		#endif
		return false;
	}

	#ifdef DEBUG_OUT 
	Serial.println("Trying to delete id: " + delRegId);
	#endif
	// Search for id entry
	boolean foundIt = false;
	for (int i=0; i<regDevNum; i++) {
		if (regAndroidIds[i] == delRegId) {
			foundIt = true;
			delRegIndex = i;
			break;
		}
	}
	if (foundIt) {
		// delete id entry
		regAndroidIds[delRegIndex] = "";
		for (int i=delRegIndex; i<regDevNum-1; i++) {
			// Shift other ids 
			regAndroidIds[i] = regAndroidIds[i+1];
		}
		regDevNum -= 1;
		if (!writeRegIds()) {
			#ifdef DEBUG_OUT 
			Serial.println("Could not save registered ids");
			#endif
			return false;
		} else {
			#ifdef DEBUG_OUT 
			Serial.println("Successful deleted id: " + delRegId);
			#endif
			return true;
		}
	} else {
		#ifdef DEBUG_OUT 
		Serial.println("Could not find registration id " + delRegId);
		#endif
		failReason = "Could not find registration id " + delRegId;
		return false;
	}
	return false;
}

/**
	delRegisteredDevice
	deletes a registration id from the list
	delRegIndex => index of id to be deleted
	returns true if successful, false if a file error occured or index is invalid
*/
boolean delRegisteredDevice(int delRegIndex) {
	#ifdef DEBUG_OUT Serial.println("===========================================");
	Serial.println("delRegisteredDevice");
	Serial.println("===========================================");
	#endif

	// First get registered devices from the files
	if (!getRegisteredDevices) {
		#ifdef DEBUG_OUT 
		Serial.println("Error reading registered devices");
		#endif
		return false;
	}

	#ifdef DEBUG_OUT 
	Serial.println("Trying to delete id with index: " + String(delRegIndex));
	#endif
	// delete id entry
	String delRegId = regAndroidIds[delRegIndex];
	regAndroidIds[delRegIndex] = "";
	for (int i=delRegIndex; i<regDevNum-1; i++) {
		// Shift other ids 
		regAndroidIds[i] = regAndroidIds[i+1];
	}
	regDevNum -= 1;
	if (!writeRegIds()) {
		#ifdef DEBUG_OUT 
		Serial.println("Could not save registered ids");
		#endif
		return false;
	} else {
		#ifdef DEBUG_OUT 
		Serial.println("Successful deleted id: " + delRegId);
		#endif
		return true;
	}
	return false;
}

/**
	gcmSendMsg
	sends message to https://android.googleapis.com/gcm/send to
		request a push notification to registered Android devices
	pushMessageIds - Json array with the keys (aka names) of the messages
	pushMessages - Json array with the messages
		a message can be any type of variable: 
		boolean, char, integer, long, float, string, ...
	returns true if successful, false if an error occured
*/
boolean gcmSendMsg(JsonArray& pushMessageIds, JsonArray& pushMessages) {
	#ifdef DEBUG_OUT 
	Serial.println("===========================================");
	Serial.println("gcmSendMsg");
	Serial.println("===========================================");
	#endif
	
	// Update list of registered devices
	getRegisteredDevices();

	if (regDevNum == 0) { // Any devices already registered?
		#ifdef DEBUG_OUT 
		Serial.println("No registered devices");
		#endif
		failReason = "No registered devices";
		return false;
	}
	
	if (pushMessageIds.size() != pushMessages.size()) {
		#ifdef DEBUG_OUT 
		Serial.println("Different number of keys and messages");
		#endif
		failReason = "Different number of keys and messages";
		return false;
	}
	
	int numData = pushMessageIds.size();

	String data;
	// Create JSON object with registration_ids and data
	if (numData != 0) {
		DynamicJsonBuffer jsonBuffer;
		DynamicJsonBuffer jsonDataBuffer;

		// Prepare JSON object
		JsonObject& msgJSON = jsonBuffer.createObject();
		// Add registration ids to JsonArray regIdArray in the JsonObject msgJSON
		JsonArray& regIdArray = msgJSON.createNestedArray("registration_ids");
		for (int i=0; i<regDevNum; i++) {
			regIdArray.add(regAndroidIds[i]);
		}
		// Add message keys and messages to JsonObject dataArray
		JsonObject& dataArray = jsonDataBuffer.createObject();
		for (int i=0; i<numData; i++) {
			String keyStr = pushMessageIds.get(i);
			dataArray[keyStr] = pushMessages.get(i);
		}
		// Add JsonObject dataArray to JsonObject msgJSON
		msgJSON["data"] = dataArray;
		msgJSON.printTo(data);
	} else { // No data to send
		#ifdef DEBUG_OUT 
		Serial.println("No data to send");
		#endif
		failReason = "No data to send";
		return false;
	}

	gcmClient.stop(); // Just to be sure
	#ifdef DEBUG_OUT 
	Serial.print("Connecting to ");
	Serial.println(serverName);
	#endif
	String resultJSON; // For later use
	if (gcmClient.connect(serverName, 80)) {
		#ifdef DEBUG_OUT 
		Serial.println("Connected to GCM server");
		#endif
		
		// String postStr = "POST /gcm/send HTTP/1.1\r\n";
		String postStr = "POST /gcm/send HTTP/1.1\r\n";
		postStr += "Host: " + hostName + "\r\n";
		postStr += "Accept: */";
		postStr += "*\r\n";
		postStr += "Authorization: key=" + API_key + "\r\n"; // Here the application specific API key is inserted
		postStr += "Content-Type: application/json\r\n";
		postStr += "Content-Length: ";
		postStr += String(data.length());
		postStr += "\r\n\r\n";
		postStr += data; // Here the Json object with the IDs and data is inserted
		postStr += "\r\n\r\n";
		
		#ifdef DEBUG_OUT 
		Serial.print("This is the Request to ");
		Serial.print(serverName);
		Serial.println(" :");
		Serial.println(postStr); 
		#endif
		gcmClient.println(postStr); // Here we send the complete POST request to the GCM server
	
		// Get response from GCM server
		#ifdef DEBUG_OUT 
		Serial.println("Response from GCM server:");
		#endif
		String inString = "";
		while (gcmClient.available()) 
		{
			char inChar = gcmClient.read();
			inString += (char)inChar;
		}
		if (inString.indexOf("200 OK") >= 0) {
			int startOfJSON = inString.indexOf("{\"multicast_id");
			int endOfJSON = inString.indexOf("}]}");
			resultJSON = inString.substring(startOfJSON,endOfJSON+3);
			#ifdef DEBUG_OUT 
			Serial.println("Found JSON result: " + resultJSON);
			#endif
			failReason = "success";
		} else {
			#ifdef DEBUG_OUT 
			Serial.println("Didn't find 200 OK");
			#endif
			failReason = "Sending not successful";
		}
		gcmClient.stop();
	} else {
		#ifdef DEBUG_OUT 
		Serial.println("Connection to GCM server failed!");
		#endif
		failReason = "Connection to GCM server failed!";
	}

	if (failReason != "success") {
		failReason = resultJSON;
		return false;
	} else {
		failReason = resultJSON;
		return true;
	}
}
