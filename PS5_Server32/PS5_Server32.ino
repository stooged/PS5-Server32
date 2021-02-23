#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <HTTPSServer.hpp>
#include <HTTPServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <DNSServer.h>
#include "cert.h"
#include "private_key.h"


String AP_SSID = "PS5_WEB_AP";
String AP_PASS = "password";

IPAddress Server_IP(10,1,1,1);
IPAddress Subnet_Mask(255,255,255,0);

DNSServer dnsServer;
using namespace httpsserver;

SSLCert cert = SSLCert(crt_DER,crt_DER_len,key_DER,key_DER_len);
HTTPSServer secureServer = HTTPSServer(&cert);
HTTPServer insecureServer = HTTPServer();


void handleHTTP(HTTPRequest * req, HTTPResponse * res) {

  String path = req->getRequestString().c_str();
  String dataType = "text/plain";
  if (path.endsWith("/")) {
    path += "index.html";
  }
  if (path.endsWith(".src")) {
    path = path.substring(0, path.lastIndexOf("."));
  } else if (path.endsWith(".html")) {
    dataType = "text/html";
  } else if (path.endsWith(".css")) {
    dataType = "text/css";
  } else if (path.endsWith(".js")) {
    dataType = "application/javascript";
  } else if (path.endsWith(".png")) {
    dataType = "image/png";
  } else if (path.endsWith(".gif")) {
    dataType = "image/gif";
  } else if (path.endsWith(".jpg")) {
    dataType = "image/jpeg";
  } else if (path.endsWith(".ico")) {
    dataType = "image/x-icon";
  } 

  File dataFile = SD.open(path.c_str());
  if (dataFile.isDirectory()) {
    dataFile.close();
    path = "/index.html";
    dataType = "text/html";
    dataFile = SD.open(path.c_str());
  }


  if (!dataFile && path.equals("/index.html")) {
    String defaultIndex = "<!DOCTYPE html><html><head><style>body { background-color: #1451AE;color: #ffffff;font-size: 14px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>ESP32</center></html>";
    res->setHeader("Content-Type", "text/html");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->setHeader("Content-Length", String(defaultIndex.length()).c_str());
    res->println(defaultIndex.c_str());
    return;
  }


  if (!dataFile) {
    req->discardRequestBody();
    res->setStatusCode(404);
    res->setStatusText("Not Found");
    res->setHeader("Content-Type", "text/html");
    res->println("<!DOCTYPE html>");
    res->println("<html>");
    res->println("<head><title>Not Found</title></head>");
    res->println("<body><h1>404 Not Found</h1><p>The requested resource was not found on this server.</p></body>");
    res->println("</html>");     
    return;
  }

  int filesize = dataFile.size();
  res->setHeader("Content-Type", dataType.c_str());
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->setHeader("Content-Length", String(filesize).c_str());

  uint8_t buf[256];  
  while (dataFile.available()) {
    int n = dataFile.read(buf, sizeof(buf));
    for(int i = 0; i < n; i++) {
      res->write(buf[i]);
    }
  }
  dataFile.close();
}

void handleHTTPS(HTTPRequest * req, HTTPResponse * res) {
  std::string serverIP = Server_IP.toString().c_str();
  req->discardRequestBody();
  res->setStatusCode(301);
  res->setStatusText("Moved Permanently");
  res->setHeader("Location", "http://" + serverIP + "/index.html");
}


void setup() {

  Serial.begin(115200);

  if (!SD.begin(SS)) {
    Serial.println("SD failed");
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str());
  WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
  Serial.println("WIFI AP started");

  dnsServer.setTTL(30);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(53, "*", Server_IP);
  Serial.println("DNS server started");

  ResourceNode * nhttp  = new ResourceNode("", "GET", &handleHTTP);
  ResourceNode * nhttps  = new ResourceNode("", "GET", &handleHTTPS);

  secureServer.setDefaultNode(nhttps);
  insecureServer.setDefaultNode(nhttp);
  secureServer.start();
  insecureServer.start();
  
  if (secureServer.isRunning() && insecureServer.isRunning()) {
    Serial.println("HTTP Server started");
  }

}

void loop() {
  dnsServer.processNextRequest();
  secureServer.loop();
  insecureServer.loop();
}
