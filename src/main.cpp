#include <Arduino.h>
#include <SPI.h>
#include <utility/spi_drv.h>
#include <WiFiNINA.h>
#include "secrets.h"
#include <I2S.h>

char ssid[] = SECRET_SSID;        
char pass[] = SECRET_PASS;    
int status = WL_IDLE_STATUS;

void printMacAddress(byte mac[]) {
  for (int i = 5; i >= 0; i--) {
    if (mac[i] < 16) {
      Serial.print("0");
    }
    Serial.print(mac[i], HEX);
    if (i > 0) {
      Serial.print(":");
    }
  }
  Serial.println();
}

void printCurrentNet() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  byte bssid[6];
  WiFi.BSSID(bssid);
  Serial.print("BSSID: ");
  printMacAddress(bssid);

  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI): ");
  Serial.println(rssi);

  byte encryption = WiFi.encryptionType();
  Serial.print("Encryption Type: ");
  Serial.println(encryption, HEX);
  Serial.println();
}

void printWiFiData() {
  IPAddress ip = WiFi.localIP();
  Serial.print("IP address : ");
  Serial.println(ip);

  Serial.print("Subnet mask: ");
  Serial.println((IPAddress)WiFi.subnetMask());

  Serial.print("Gateway IP : ");
  Serial.println((IPAddress)WiFi.gatewayIP());

  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC address: ");
  printMacAddress(mac);
}

void sendIPAddress(String ipAddress) {
  IPAddress ip;
  ip.fromString(ipAddress);
  SpiDrv::spiTransfer(ip[0]);
  SpiDrv::spiTransfer(ip[1]);
  SpiDrv::spiTransfer(ip[2]);
  SpiDrv::spiTransfer(ip[3]);
}

void sendPort(uint16_t port) {
  byte a=(port&0xFF);
  byte b=((port>>8)&0xFF);
  SpiDrv::spiTransfer(a);
  SpiDrv::spiTransfer(b);
}

void setIPAddressPortCommand() {
  WAIT_FOR_SLAVE_SELECT();
  SpiDrv::spiTransfer(0xD1);
  sendIPAddress(LISTENER_IP);
  sendPort(LISTENER_PORT);
  SpiDrv::spiTransfer(0xD2);
  SpiDrv::spiSlaveDeselect();

  
  SpiDrv::waitForSlaveReady();
  SpiDrv::spiSlaveSelect();

  // Wait for reply
  char responseByte1 = SpiDrv::spiTransfer(DUMMY_DATA);
  char responseByte2 = SpiDrv::spiTransfer(DUMMY_DATA);

  SpiDrv::spiSlaveDeselect();

  if (responseByte1 == 'A' && responseByte2 == 'B') {
    Serial.print("Have set listener ipaddress and port: ");
    Serial.print(LISTENER_IP);
    Serial.print(":");
    Serial.print(LISTENER_PORT);
    Serial.println();
    return;
  }
  while (true) {
    Serial.print("Listener ipaddress and port not set, aborted!");
    delay(2000);
  }
}

void sendIntValue(int value) {
  byte a,b,c,d;
       
  a=(value&0xFF);
  b=((value>>8)&0xFF);
  c=((value>>16)&0xFF);
  d=((value>>24)&0xFF);

  SpiDrv::spiTransfer(a);
  SpiDrv::spiTransfer(b);
  SpiDrv::spiTransfer(c);
  SpiDrv::spiTransfer(d);
}

void sendBuffer(int32_t* buffer, int length) {
  WAIT_FOR_SLAVE_SELECT();
  for(int i=0; i<length; i++) {
    int value = buffer[i];
    if(value != 0) {
      value = value ^ CYPHER_KEY;
      sendIntValue(value);
    }
  }
  SpiDrv::spiSlaveDeselect();
}

void setup() {
  delay(1000);
  
  Serial.begin(9600);

  while (!Serial) {
    ; // wait for serial port to connect
  }

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  while ( status != WL_CONNECTED) {
    Serial.print("Attempting to connect to WPA SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(5000);
  }

  Serial.println("You're connected to the network");

  setIPAddressPortCommand();

  if (!I2S.begin(I2S_PHILIPS_MODE, 44100, 32)) {
    Serial.println("Failed to initialize I2S!");
    while (1);
  }

  Serial.println("Initialized I2S - sending data to port");
}
 
byte i2sDataBuffer[1024];
void loop() {
  int size = I2S.available();
  I2S.read(i2sDataBuffer, size);
  if (size < 4) {
    return;
  }
  sendBuffer((int32_t*)i2sDataBuffer, size/4);
}
