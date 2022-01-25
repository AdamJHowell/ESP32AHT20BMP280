/*
 * This sketch is a branc of my PubSubWeather sketch.
 * This sketch will use a AHT20/BMP280 combination sensor to show temperature, pressure, and humidity.
 * The ESP-32 SDA pin is GPIO21, and SCL is GPIO22.
 */
#include "WiFi.h"						// This header is part of the standard library.  https://www.arduino.cc/en/Reference/WiFi
#include <Wire.h>						// This header is part of the standard library.  https://www.arduino.cc/en/reference/wire
#include <PubSubClient.h>			// PubSub is the MQTT API.  Author: Nick O'Leary  https://github.com/knolleary/pubsubclient
#include <SparkFun_Qwiic_Humidity_AHT20.h>	// Library used to interface with the AHT20.  Author: SparkFun  https://github.com/sparkfun/SparkFun_Qwiic_Humidity_AHT20_Arduino_Library
#include "Seeed_BMP280.h"							// https://github.com/Seeed-Studio/Grove_BMP280
#include "privateInfo.h"			// I use this file to hide my network information from random people browsing my GitHub repo.

/**
 * Declare network variables.
 * Adjust the commented-out variables to match your network and broker settings.
 * The commented-out variables are stored in "privateInfo.h", which I do not upload to GitHub.
 */
//const char* wifiSsid = "yourSSID";				// Typically kept in "privateInfo.h".
//const char* wifiPassword = "yourPassword";		// Typically kept in "privateInfo.h".
//const char* mqttBroker = "yourBrokerAddress";	// Typically kept in "privateInfo.h".
//const int mqttPort = 1883;							// Typically kept in "privateInfo.h".
const char* mqttTopic = "ajhWeather";
const String sketchName = "ESP32AHT20BMP280.ino";
char ipAddress[16];
char macAddress[18];
int loopCount = 0;
int mqttPublishDelayMS = 60000;
int pirPin = 8;											// The GPIO that the PIR "out" pin is connected to.

// Create class objects.
WiFiClient espClient;							// Network client.
PubSubClient mqttClient( espClient );		// MQTT client.
AHT20 aht20Sensor;
BMP280 bmp280Sensor;


/**
 * The setup() function runs once when the device is booted, and then loop() takes over.
 */
void setup()
{
	// Start the Serial communication to send messages to the computer.
	Serial.begin( 115200 );
	delay( 10 );
	Serial.println( '\n' );
	Serial.println( sketchName + " is beginning its setup()." );
	Wire.begin();	// Join I2C bus.

	// Set the ipAddress char array to a default value.
	snprintf( ipAddress, 16, "127.0.0.1" );

	Serial.println( "Initializing the BMP280 sensor..." );
	// Initialize the BMP280.
	if( !bmp280Sensor.init() )
	{
		Serial.println( "BMP280 could not be initialized!" );
	}
	else
	{
		Serial.println( "BMP280 has been initialized." );
	}

	Serial.println( "Initializing the AHT20 sensor..." );
	// Initialize the AHT20.
	if( aht20Sensor.begin() == false )
	{
		Serial.println( "AHT20 not detected. Please check wiring." );
		while( 1 );
	}
	Serial.println( "AHT20 has been initialized." );

	// Set the MQTT client parameters.
	mqttClient.setServer( mqttBroker, mqttPort );

	// Get the MAC address and store it in macAddress.
	snprintf( macAddress, 18, "%s", WiFi.macAddress().c_str() );

	// Try to connect to the configured WiFi network, up to 10 times.
	wifiConnect( 20 );
} // End of setup() function.


void wifiConnect( int maxAttempts )
{
	// Announce WiFi parameters.
	String logString = "WiFi connecting to SSID: ";
	logString += wifiSsid;
	Serial.println( logString );

	// Connect to the WiFi network.
	Serial.printf( "Wi-Fi mode set to WIFI_STA %s\n", WiFi.mode( WIFI_STA ) ? "" : " - Failed!" );
	WiFi.begin( wifiSsid, wifiPassword );

	int i = 0;
	/*
     WiFi.status() return values:
     0 : WL_IDLE_STATUS when WiFi is in process of changing between statuses
     1 : WL_NO_SSID_AVAIL in case configured SSID cannot be reached
     3 : WL_CONNECTED after successful connection is established
     4 : WL_CONNECT_FAILED if wifiPassword is incorrect
     6 : WL_DISCONNECTED if module is not configured in station mode
  */
	// Loop until WiFi has connected.
	while( WiFi.status() != WL_CONNECTED && i < maxAttempts )
	{
		delay( 1000 );
		Serial.println( "Waiting for a connection..." );
		Serial.print( "WiFi status: " );
		Serial.println( WiFi.status() );
		logString = ++i;
		logString += " seconds";
		Serial.println( logString );
	}

	// Print that WiFi has connected.
	Serial.println( '\n' );
	Serial.println( "WiFi connection established!" );
	Serial.print( "MAC address: " );
	Serial.println( macAddress );
	Serial.print( "IP address: " );
	snprintf( ipAddress, 16, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3] );
	Serial.println( ipAddress );
} // End of wifiConnect() function.


// mqttConnect() will attempt to (re)connect the MQTT client.
void mqttConnect( int maxAttempts )
{
	int i = 0;
	// Loop until MQTT has connected.
	while( !mqttClient.connected() && i < maxAttempts )
	{
		Serial.print( "Attempting MQTT connection..." );
		// Connect to the broker using the MAC address for a clientID.  This guarantees that the clientID is unique.
		if( mqttClient.connect( macAddress ) )
		{
			Serial.println( "connected!" );
		}
		else
		{
			Serial.print( " failed, return code: " );
			Serial.print( mqttClient.state() );
			Serial.println( " try again in 2 seconds" );
			// Wait 5 seconds before retrying.
			delay( 5000 );
		}
		i++;
	}
} // End of mqttConnect() function.


/**
 * The loop() function begins after setup(), and repeats as long as the unit is powered.
 */
void loop()
{
	loopCount++;
	int val = digitalRead( pirPin );
	if( val )
	{
		Serial.println( "Motion detected!" );
	}
	else
	{
		Serial.println( "Nothing is moving." );
	}

	Serial.println();
	// Check the mqttClient connection state.
	if( !mqttClient.connected() )
	{
		// Reconnect to the MQTT broker.
		mqttConnect( 10 );
	}
	// The loop() function facilitates the receiving of messages and maintains the connection to the broker.
	mqttClient.loop();

	// The AHT20 can respond with a reading every ~50ms.
	// However, increased read time can cause the IC to heat around 1.0C above ambient temperature.
	// The datasheet recommends reading every 2 seconds.
	if( aht20Sensor.available() == true )
	{
		// Get temperature and humidity data from the AHT20.
		float ahtTemp = aht20Sensor.getTemperature();
		float ahtHumidity = aht20Sensor.getHumidity();

		// Get temperature and humidity data from the BMP280.
		float bmpTemp = bmp280Sensor.getTemperature();
		float bmpPressure = bmp280Sensor.getPressure();
		float bmpAlt = bmp280Sensor.calcAltitude( bmpPressure );

		// Print the AHT20 data.
		Serial.print( "AHT20 Temperature:   " );
		Serial.print( ahtTemp, 2 );
		Serial.print( " C\t" );
		Serial.print( "Humidity: " );
		Serial.print( ahtHumidity, 2 );
		Serial.print( "% RH" );
		Serial.println();

		// Print the BMP280 data.
		Serial.print( "BMP280 Temperature:  " );
		Serial.print( bmpTemp, 2 );
		Serial.print( " C\t" );
		Serial.print( "Pressure: " );
		Serial.print( bmpPressure, 0 );
		Serial.print( " Pa\t" );
		Serial.print( "Altitude: " );
		Serial.print( bmpAlt, 2 );
		Serial.print( " m\n" );

		Serial.print( "Average Temperature: " );
		Serial.print( ( ahtTemp + bmpTemp ) / 2, 2 );
		Serial.println( " C\n" );

		// Prepare a String to hold the JSON.
		char mqttString[256];
		// Write the readings to the String in JSON format.
		snprintf( mqttString, 256, "{\n\t\"sketch\": \"%s\",\n\t\"mac\": \"%s\",\n\t\"ip\": \"%s\",\n\t\"temp1C\": %.1f,\n\t\"humidity\": %.1f,\n\t\"temp2C\": %.1f,\n\t\"pressure\": %.1f,\n\t\"altM\": %.1f\n}", sketchName, macAddress, ipAddress, ahtTemp, ahtHumidity, bmpTemp, bmpPressure, bmpAlt );
		if( mqttClient.connected() )
		{
			// Publish the JSON to the MQTT broker.
			mqttClient.publish( mqttTopic, mqttString );
		}
		// Print the JSON to the Serial port.
		Serial.println( mqttString );
	}

	String logString = "loopCount: ";
	logString += loopCount;
	Serial.println( logString );
	Serial.println( "Pausing for 60 seconds..." );
	delay( 60000 );	// Wait for 60 seconds.
} // End of loop() function.
