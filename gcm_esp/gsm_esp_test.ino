/**
	Google Cloud Messaging tutorial
	part 2 
	-> web server for receiving registrations from Android devices
	   receive commands to maintain registration storage
	   receive command to request push notificatio
	-> setup routine
	=> main loop

	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	To access the web server from outside your local LAN you need
	to setup a url and prepare your router/modem to make this IP
	accessible from the Internet
	!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
*/

/* Includes from libraties */
#include <WiFiClient.h>
#include <Ticker.h>

/* wifiAPinfo.h contains wifi SSID and password */
/* REMARK: For privacy reasons I do not share my settings.
	file content looks like: */
/* ===FILE CONTENT BELOW=========================================== */
//	/** SSID of local WiFi network */
//	const char* ssid = "===YOUR_AP_SSID_HERE===";
//	/** Password for local WiFi network */
//	const char* password = "===YOUR_WIFI_PASSWORD_HERE===";
/* ===FILE CONTENT ABOVE=========================================== */
#include "wifiAPinfo.h"

/* REMARK: I use fixed IP address for the ESP8266 module, not the DHCP function */
/** IP address of this module */
IPAddress ipAddr(192, 168, 0, 148); // REPLACE WITH THE IP ADDRESS YOU WANT TO APPLY
/** Gateway address of WiFi access point */
IPAddress ipGateWay(192, 168, 0, 1); // REPLACE WITH THE GATEWAY ADDRESS OF YOUR WIFI AP
/** Network mask of the local lan */
IPAddress ipSubNet(255, 255, 255, 0); // SHOULD WORK WITH MOST WIFI LANs

/** WiFiServer class to create simple web server */
WiFiServer server(80);

/** Timer for flashing blue communication LED */
Ticker comFlasher;
/** Timer for sending a push request to GCM every minute */
Ticker pushTrigger;
/** Flag for push request */
boolean doPush = false;

/** Blue LED on GPIO2 for communication activities (valid only for Adadfruit HUZZAH ESP8266 breakout) */
#define comLED 2

/** Just for testing, counter to be send as message */
int msgCnt = 0;

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
			if (delRegisteredDevice(true)) {
				root["result"] = "success";
			} else {
				root["result"] = "failed";
				root["reason"] = failReason;
			}
		} else if (delReq == "i") {
			String delRegId = req.substring(5,146);
			delRegId.trim();
			if (delRegisteredDevice(delRegId)) {
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
				if (delRegisteredDevice(delRegIndex)) {
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
	// Message must be a string in valid JSON format
	// e.g. {"msgkeys":["key1","key2"],"msgs":["msg1","msg2"]}
	} else if (req.substring(0, 3) == "/?s"){
		String jsonMsgs = req.substring(4,req.length());
		jsonMsgs.trim();
		jsonMsgs.replace("%22","\""); // " might have been replaced with %22 by the browser
		jsonMsgs.replace("%20"," "); // <space> might have been replaced with %22 by the browser
		jsonMsgs.replace("%27","'"); // ' might have been replaced with %22 by the browser
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
					if (gcmSendMsg(msgKeysJSON, msgsJSON)) {
						root["result"] = "success";
					} else {
						root["response"] = failReason;
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
	// Create messages & keys as JSON arrays
	DynamicJsonBuffer jsonBufferKeys;
	DynamicJsonBuffer jsonBufferMsgs;
	JsonArray& msgKeysJSON = jsonBufferKeys.createArray();
	JsonArray& msgsJSON = jsonBufferMsgs.createArray();
	char buf[4];
	char msgOne[24] = "Test message number ";
	itoa(msgCnt,buf,10);
	strcat(msgOne,buf);
	msgKeysJSON.add("message");
	msgKeysJSON.add("timestamp");
	msgsJSON.add(msgOne);
	msgsJSON.add(buf);
	gcmSendMsg(msgKeysJSON, msgsJSON);
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
		comFlasher.attach(0.5, blueLedFlash);

		doPush = false;
		// Create messages & keys as JSON arrays
		DynamicJsonBuffer jsonBufferKeys;
		DynamicJsonBuffer jsonBufferMsgs;
		JsonArray& msgKeysJSON = jsonBufferKeys.createArray();
		JsonArray& msgsJSON = jsonBufferMsgs.createArray();
		char buf[4];
		char msgOne[24] = "Test message number ";
		itoa(msgCnt,buf,10);
		strcat(msgOne,buf);
		msgKeysJSON.add("message");
		msgKeysJSON.add("timestamp");
		msgsJSON.add(msgOne);
		msgsJSON.add(buf);
		gcmSendMsg(msgKeysJSON, msgsJSON);
		msgCnt++;
		comFlasher.detach();
		digitalWrite(comLED, HIGH); // Turn off LED
	}
}
