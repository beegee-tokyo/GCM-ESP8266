/**
	Google Cloud Messaging tutorial

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
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <Ticker.h>

/* wifiAPinfo.h contains wifi SSID and password */
/* REMARK: For privacy reasons I do not share my settings.
	file content looks like: */
/* ===FILE CONTENT BELOW=========================================== */
//	/** SSID of local WiFi network */
//	const char* ssid = "===YOUR_AP_SSID_HERE===";
//	/** Password for local WiFi network */
//	const char* password = "===YOUR_WIFI_PASSWORD_HERE===";
//	/** Google API key */
//	const String API_key = "===YOUR_GOOGLE_API_KEY_HERE===";
/* ===FILE CONTENT ABOVE=========================================== */
#include "wifiAPinfo.h"

/* Flag for Serial output */
#define DEBUG_OUT
	
/* REMARK: I use fixed IP address for the ESP8266 module, not the DHCP function */
/** IP address of this module */
IPAddress ipAddr(192, 168, 0, 148); // REPLACE WITH THE IP ADDRESS YOU WANT TO APPLY
/** Gateway address of WiFi access point */
IPAddress ipGateWay(192, 168, 0, 1); // REPLACE WITH THE GATEWAY ADDRESS OF YOUR WIFI AP
/** Network mask of the local lan */
IPAddress ipSubNet(255, 255, 255, 0); // SHOULD WORK WITH MOST WIFI LANs

/** Google Cloud Messaging server address */
char serverName[] = "http://android.googleapis.com";
/** Google Cloud Messaging host */
String hostName = "android.googleapis.com";
/** WiFiClient class to communicate with GCM server */
WiFiClient gcmClient;
/** WiFiServer class to create simple web server */
WiFiServer server(80);
/** Reason of failure of received command */
String failReason = "unknown";
/** Definition of message IDs */
String messageIDs[10] = {"message","timestamp","","","","","","","",""};
/** Message texts for the GCM push */
String messageTxts[10] = {"","","","","","","","","",""};
/** Just for testing, counter to be send as message */
int msgCnt = 0;

/** Array to hold the registered Android device IDs (max 5 devices, extend if needed)*/
#define MAX_DEVICE_NUM 5
String regAndroidIds[MAX_DEVICE_NUM];
/** Number of registered devices */
byte regDevNum = 0;

/** Blue LED on GPIO2 for communication activities (valid only for Adadfruit HUZZAH ESP8266 breakout) */
#define comLED 2

/** Timer for flashing blue communication LED */
Ticker comFlasher;
/** Timer for sending a push request to GCM every minute */
Ticker pushTrigger;
/** Flag for push request */
boolean doPush = false;

/* ===CODE_STARTS_HERE========================================== */
/**
	blueLedFlash
	called by Ticker comFlasher
	change status of led on each call
*/
void blueLedFlash() {
	int state = digitalRead(comLED);
	digitalWrite(comLED, !state);
}

/**
	triggerGCM
	called by Ticker pushTrigger
	sets flag doPush to true for handling in loop()
	will initiate a call to gcmSendMsg() from loop()
*/
void triggerGCM() {
	doPush = true;
}

/**
	writeRegIds
	writes regAndroidIds[] to file gcm.txt as JSON object
	returns true if successful, false if file error
*/
boolean writeRegIds() {
	// Open config file for writing.
	File devicesFile = SPIFFS.open("/gcm.txt", "w");
	if (!devicesFile)
	{
		#ifdef DEBUG_OUT 
		Serial.println("Failed to open gcm.txt for writing");
		#endif
		failReason = "Failed to open gcm.txt for writing";
		return false;
	}
	// Create new device ids as JSON
	DynamicJsonBuffer jsonBuffer;

	// Prepare JSON object
	JsonObject& regJSON = jsonBuffer.createObject();

	// Add existing devices to JSON object (if any)
	for (int i=0; i<regDevNum; i++) {
		regJSON[String(i)] = regAndroidIds[i];
	}
	
	// Convert JSON object to String
	String jsonTxt;
	regJSON.printTo(jsonTxt);

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
	// First get registered devices from the files
	// open file for reading.
	File devicesFile = SPIFFS.open("/gcm.txt", "r");
	if (devicesFile) // Found file
	{
		// Read content from config file.
		String content = devicesFile.readString();
		devicesFile.close();
	
		// Convert file content into JSON object
		DynamicJsonBuffer jsonBuffer;
		JsonObject& regJSON = jsonBuffer.parseObject(content);
		if (regJSON.success()) // Found some entries
		{
			regDevNum = 0;
			for (int i=0; i<MAX_DEVICE_NUM; i++) {
				if (regJSON.containsKey(String(i))) { // Found an entry
					regAndroidIds[i] = regJSON[String(i)].asString();
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
	} else { // File not found
		#ifdef DEBUG_OUT 
		Serial.println("File does not exist");
		#endif
		failReason = "File does not exist";
		return false;
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
	deletes a registration id from the list
	delAll => if true deletes all registration ids
	delRegId => registration id to be deleted. delAll must be false & delRegIndex must be 9999!!!!
	delRegIndex => id with index will be deleted
	returns true if successful, false if a file error occured or ID/index is invalid or not registered 
*/
boolean delRegisteredDevice(boolean delAll, String delRegId, int delRegIndex) {
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

	if (delAll) { // Delete all entries
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
	} else if (delRegIndex <= regDevNum-1) { // Delete specific id by index
		#ifdef DEBUG_OUT 
		Serial.println("Trying to delete id with index: " + String(delRegIndex));
		#endif
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
	} else if (delRegId.length() != 0) { // Delete specific id by id value
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
	}
	return false;
}

/**
	gcmSendMsg
	sends message to https://android.googleapis.com/gcm/send to
		request a push notification to registered Android devices
	returns true if successful, false if an error occured
*/
boolean gcmSendMsg(int numData, String pushMessageIds[], String pushMessages[]) {
	#ifdef DEBUG_OUT Serial.println("===========================================");
	Serial.println("gcmSendMsg");
	Serial.println("===========================================");
	#endif
	comFlasher.attach(0.5, blueLedFlash);
	
	// Update list of registered devices
	getRegisteredDevices();

	if (regDevNum == 0) { // Any devices already registered?
		#ifdef DEBUG_OUT 
		Serial.println("No registered devices");
		#endif
		comFlasher.detach();
		digitalWrite(comLED, HIGH); // Turn off LED
		failReason = "No registered devices";
		return false;
	}
	
	String data;
	// Create JSON object with registration_ids and data
	if (numData != 0) {
		DynamicJsonBuffer jsonBuffer;
		DynamicJsonBuffer jsonDataBuffer;

		// Prepare JSON object
		JsonObject& regJSON = jsonBuffer.createObject();
		JsonArray& regIdArray = regJSON.createNestedArray("registration_ids");
		for (int i=0; i<regDevNum; i++) {
			regIdArray.add(regAndroidIds[i]);
		}
		JsonObject& dataArray = jsonDataBuffer.createObject();
		for (int i=0; i<numData; i++) {
			dataArray[pushMessageIds[i]] = pushMessages[i];
		}
		regJSON["data"] = dataArray;
		regJSON.printTo(data);
	} else { // No data to send
		#ifdef DEBUG_OUT 
		Serial.println("No data to send");
		#endif
		comFlasher.detach();
		digitalWrite(comLED, HIGH); // Turn off LED
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
		postStr += "Authorization: key=" + API_key + "\r\n";
		postStr += "Content-Type: application/json\r\n";
		postStr += "Content-Length: ";
		postStr += String(data.length());
		postStr += "\r\n\r\n";
		postStr += data;
		postStr += "\r\n\r\n";
		
		#ifdef DEBUG_OUT 
		Serial.print("This is the Request to ");
		Serial.print(serverName);
		Serial.println(" :");
		Serial.println(postStr); 
		#endif
		gcmClient.println(postStr);
	
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
	comFlasher.detach();
	digitalWrite(comLED, HIGH); // Turn off LED
	if (failReason != "success") {
		return false;
	} else {
		failReason = resultJSON;
		return true;
	}
}

/**
	webServer
	receives commands from web client or Android device
	returns result as JSON string to the client
		"result":"success" ==> command successful
		"result":"timeout" ==> communication between client and server was interrupted
		"result":"invalid" ==> unknown command or invalid Android ID
		"result":"failed" ==> command unsuccessful because e.g. ID storage full, file error
		
	===================
	Available commands:
	===================
	-- Register a new id
		<url>/?regid=<android registration id> with length 140 letter, no leading/trailing <">
	-- List registered ids
		<url>/?l
	-- Delete all registered ids
		<url>/?da
	-- Delete specific registered id by 140 letter registration id
		<url>/?di=<android registration id> with length 140 letter, no leading/trailing <">
	-- Delete specific registered index
		<url>/?dx=<index>
	-- Send broadcast message to all registered devices
		<url>/?s={"msgkeys":["key1","key2"],"msgs":["msg1","msg2"]}
	=========
	Examples:
	=========
	192.168.0.148/?l
		returns
		{"result":"success","0":"***ANDROID_REG_ID***","num":1}
	192.168.0.148/?regid=APA91bHq3iaH0UqYadNegxi5lP0Li0lvumQvvKJS7GxtT9P0zkU-...-S6EOpbS
		returns
		{"result":"success","0":"***ANDROID_REG_ID***","num":1}
	192.168.0.148/?da
		returns
		{"result":"success"}
	192.168.0.148/?s={"msgkeys":["message","timestamp"],"msgs":["Hello world","Christmas 2015"]}
		sends {"message":"Hello world","timestamp":"Christmas 2015"} to all registered devices
		returns		{"result":"success","response":"{\"multicast_id\":6404311408057644811,\"success\":3,\"failure\":0,\"canonical_ids\":0,\"results\":[{\"message_id\":\"0:1453965670295623%adf4b72ff9fd7ecd\"},{\"message_id\":\"0:1453965670295621%adf4b72ff9fd7ecd\"},{\"message_id\":\"0:1453965670295907%cbe2ca3ff9fd7ecd\"}]}"}
*/
void webServer(WiFiClient httpClient) {
	#ifdef DEBUG_OUT Serial.println("===========================================");
	Serial.println("webServer");
	Serial.println("===========================================");
	#endif
	comFlasher.attach(0.5, blueLedFlash);
	String s = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\nContent-Type: application/json\r\n\r\n";
	int waitTimeOut = 0;

	DynamicJsonBuffer jsonBuffer;
	
	// Prepare json object for the response
	JsonObject& root = jsonBuffer.createObject();

	// Wait until the client sends some data
	while (!httpClient.available()) {
		delay(1);
		waitTimeOut++;
		if (waitTimeOut > 3000) { // If no response for 3 seconds return
			#ifdef DEBUG_OUT 
			Serial.println("Timeout while waiting for data from client");
			#endif
			root["result"] = "timeout";
			root["reason"] = "Timeout while waiting for data from client";
			String jsonString;
			root.printTo(jsonString);
			s += jsonString;
			httpClient.print(s);
			httpClient.flush();
			httpClient.stop();
			comFlasher.detach();
			digitalWrite(comLED, HIGH); // Turn off LED
			return;
		}
	}

	root["result"] = "success";
	
	// Read the first line of the request
	String req = httpClient.readStringUntil('\r');
	#ifdef DEBUG_OUT 
	Serial.println("Received: "+req);
	#endif
	req = req.substring(req.indexOf("/"),req.length()-9);
	#ifdef DEBUG_OUT 
	Serial.println("Trimmed command = "+req);
	#endif
	// Registration of new device
	if (req.substring(0, 8) == "/?regid=") {
		String regID = req.substring(8,req.length());
		#ifdef DEBUG_OUT Serial.println("RegID: "+regID);
		Serial.println("Length: "+String(regID.length()));
		#endif
		// Check if length of ID is correct
		if (regID.length() != 140) {
			#ifdef DEBUG_OUT 
			Serial.println("Length of ID is wrong");
			#endif
			root["result"] = "invalid";
			root["reason"] = "Length of ID is wrong";
		} else {
			// Try to save ID 
			if (!addRegisteredDevice(regID)) {
				#ifdef DEBUG_OUT 
				Serial.println("Failed to save ID");
				#endif
				root["result"] = "failed";
				root["reason"] = failReason;
			} else {
				#ifdef DEBUG_OUT 
				Serial.println("Successful saved ID");
				#endif
				root["result"] = "success";
				getRegisteredDevices();
				for (int i=0; i<regDevNum; i++) {
					root[String(i)] = regAndroidIds[i];
				}
				root["num"] = regDevNum;
			}
		}
	// Send list of registered devices
	} else if (req.substring(0, 3) == "/?l"){
		if (getRegisteredDevices()) {
			if (regDevNum != 0) { // Any devices already registered?
				for (int i=0; i<regDevNum; i++) {
					root[String(i)] = regAndroidIds[i];
				}
			}
			root["num"] = regDevNum;
			root["result"] = "success";
		} else {
			root["result"] = "failed";
			root["reason"] = failReason;
		}
	// Delete one or all registered device
	} else if (req.substring(0, 3) == "/?d"){
		String delReq = req.substring(3,4);
		if (delReq == "a") { // Delete all registered devices
			if (delRegisteredDevice(true, "", 9999)) {
				root["result"] = "success";
			} else {
				root["result"] = "failed";
				root["reason"] = failReason;
			}
		} else if (delReq == "i") {
			String delRegId = req.substring(5,146);
			delRegId.trim();
			if (delRegisteredDevice(false, delRegId, 9999)) {
				root["result"] = "success";
			} else {
				root["result"] = "failed";
				root["reason"] = failReason;
			}
		} else if (delReq == "x") {
			int delRegIndex = req.substring(5,req.length()).toInt();
			if ((delRegIndex < 0) || (delRegIndex > MAX_DEVICE_NUM-1)) {
				root["result"] = "invalid";
				root["reason"] = "Index out of range";
			} else {
				if (delRegisteredDevice(false, "", delRegIndex)) {
					root["result"] = "success";
				} else {
					root["result"] = "failed";
					root["reason"] = failReason;
				}
			}
		}
		// Send list of registered devices
		if (getRegisteredDevices()) {
			if (regDevNum != 0) { // Any devices already registered?
				for (int i=0; i<regDevNum; i++) {
					root[String(i)] = regAndroidIds[i];
				}
			}
			root["num"] = regDevNum;
		}
	// Send broadcast message to all registered devices
	// Message must be in JSON format
	// e.g. {"msgkeys":["key1","key2"],"msgs":["msg1","msg2"]}
	} else if (req.substring(0, 3) == "/?s"){
		String jsonMsgs = req.substring(4,req.length());
		jsonMsgs.trim();
		jsonMsgs.replace("%22","\"");
		jsonMsgs.replace("%20"," ");
		jsonMsgs.replace("%27","'");
		#ifdef DEBUG_OUT 
		Serial.println("Received = " + jsonMsgs);
		#endif
		// Convert message into JSON object
		DynamicJsonBuffer jsonBuffer;
		JsonObject& regJSON = jsonBuffer.parseObject(jsonMsgs);
		if (regJSON.success()) {// Found some entries
			root["result"] = "invalid";
			if (regJSON.containsKey("msgkeys")) {
				if (regJSON.containsKey("msgs")) {
					JsonArray& msgKeysJSON = regJSON["msgkeys"];
					JsonArray& msgsJSON = regJSON["msgs"];
					if (msgKeysJSON.size() == msgsJSON.size()) {
						for (int i=0; i<msgsJSON.size(); i++) {
							messageIDs[i] = msgKeysJSON.get<String>(i);
							messageTxts[i] = msgsJSON.get<String>(i);
							#ifdef DEBUG_OUT 
							Serial.print("Key = " + messageIDs[i]);
							Serial.println(" Msg = " + messageTxts[i]);
							#endif
						}
						if (gcmSendMsg(msgsJSON.size(), messageIDs, messageTxts)) {
							root["result"] = "success";
							//JsonObject& resultJSON = jsonBuffer.parseObject(failReason);
							//if (resultJSON.success()) {
								root["response"] = failReason;
							//}
						}
						root["result"] = "success";
					} else {
						root["reason"] = "Number of keys not equal to number of messages";
					}
				} else {
					root["reason"] = "JSON key msgs is missing";
				}
			} else {
				root["reason"] = "JSON key msgkeys is missing";
			}
		} else {
			root["result"] = "invalid";
			root["reason"] = "Invalid JSON message";
		}
	} else {
		root["result"] = "invalid";
		root["reason"] = "Invalid command";
	}
	
  	String jsonString;
  	root.printTo(jsonString);
  	s += jsonString;
  	httpClient.print(s);
  	httpClient.flush();
  	httpClient.stop();

	comFlasher.detach();
	digitalWrite(comLED, HIGH); // Turn off LED
}

/**
	setup
	Initialize application
	called once after reboot
*/
void setup() {
	pinMode(comLED, OUTPUT); // Communication LED blue
	digitalWrite(comLED, HIGH); // Turn off LED
	#ifdef DEBUG_OUT 
	Serial.begin(115200);
	Serial.setDebugOutput(false);
	Serial.println("");
	Serial.println("====================");
	Serial.println("ESP8266 GCM test");
	#endif
	comFlasher.attach(0.5, blueLedFlash);
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);
	WiFi.config(ipAddr, ipGateWay, ipSubNet);
	WiFi.begin(ssid, password);
	#ifdef DEBUG_OUT 
	Serial.print("Waiting for WiFi connection ");
	#endif
	int connectTimeout = 0;
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		#ifdef DEBUG_OUT
		Serial.print(".");
		#endif
		connectTimeout++;
		if (connectTimeout > 60) { //Wait for 30 seconds (60 x 500 milliseconds) to reconnect
			pinMode(16, OUTPUT); // Connected to RST pin
			digitalWrite(16,LOW); // Initiate reset
			ESP.reset();
		}
	}
	comFlasher.detach();
	digitalWrite(comLED, HIGH); // Turn off LED

	#ifdef DEBUG_OUT 
	Serial.println("");
	Serial.print("Connected to ");
	Serial.println(ssid);
	Serial.print("IP address: ");
	Serial.println(WiFi.localIP());

	byte mac[6];
	WiFi.macAddress(mac);
	String localMac = String(mac[0], HEX) + ":";
	localMac += String(mac[1], HEX) + ":";
	localMac += String(mac[2], HEX) + ":";
	localMac += String(mac[3], HEX) + ":";
	localMac += String(mac[4], HEX) + ":";
	localMac += String(mac[5], HEX);

	Serial.print("MAC address: ");
	Serial.println(localMac);

	Serial.print("Sketch size: ");
	Serial.print (ESP.getSketchSize());
	Serial.print(" - Free size: ");
	Serial.println(ESP.getFreeSketchSpace());
	Serial.println("====================");
	#endif

	// Initialize file system.
	if (!SPIFFS.begin())
	{
		#ifdef DEBUG_OUT 
		Serial.println("Failed to mount file system");
		#endif
	}

	// Start the web server to serve incoming requests
	server.begin();
	
	// Start the timer to push a message every minute
	pushTrigger.attach(60, triggerGCM);
	
	// Send first push notification
	messageTxts[0] = "Test message number " + String(msgCnt);
	messageTxts[1] = String(msgCnt);
	gcmSendMsg(2, messageIDs, messageTxts);
	msgCnt++;
}

/**
	loop
	main program loop
	wait for connection to web server
	for testing purpose sends every minute a GCM notification to
		registered Android devices
*/
void loop() {
	// Handle new client request on HTTP server if available
	WiFiClient client = server.available();
	if (client) {
		webServer(client);
	}
	
	if (doPush) {
		doPush = false;
		messageTxts[0] = "Test message number " + String(msgCnt);
		messageTxts[1] = String(msgCnt);
		gcmSendMsg(2, messageIDs, messageTxts);
		#ifdef DEBUG_OUT 
		Serial.println(failReason);
		#endif
		msgCnt++;
	}
}
