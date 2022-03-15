#include <FS.h>
#include <WiFi.h>
#include <HTTPSServer.hpp>
#include <HTTPServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <HTTPURLEncodedBodyParser.hpp>
#include <HTTPBodyParser.hpp>
#include <HTTPMultipartBodyParser.hpp>
#include <ESPmDNS.h>
#include <Update.h>
#include <DNSServer.h>
#include "cert.h"
#include "private_key.h"
#include "jzip.h"

#if defined(CONFIG_IDF_TARGET_ESP32S2) | defined(CONFIG_IDF_TARGET_ESP32S3) // ESP32-S2/S3 BOARDS(usb emulation)
#include "USB.h"
#include "USBMSC.h"
#include "exfathax.h"
#elif defined(CONFIG_IDF_TARGET_ESP32) // ESP32 BOARDS
#define USBCONTROL false               // set to true if you are using usb control(wired up usb drive)
#define usbPin 4                       // set the pin you want to use for usb control
#else
#error "Selected board not supported"
#endif

                    // use SD Card [ true / false ]
#define USESD false // a FAT32 formatted SD Card will be used instead of the onboard flash for the storage.
                    // this requires a board with a sd card slot or a sd card connected.

                     // use FatFS not SPIFFS [ true / false ]
#define USEFAT false // FatFS will be used instead of SPIFFS for the storage filesystem or for larger partitons on boards with more than 4mb flash.
                     // you must select a partition scheme labeled with "FAT" or "FATFS" with this enabled.

                     // use LITTLEFS not SPIFFS [ true / false ]
#define USELFS false // LITTLEFS will be used instead of SPIFFS for the storage filesystem.
                     // you must select a partition scheme labeled with "SPIFFS" with this enabled and USEFAT must be false.

#if USESD
#include "SD.h"
#include "SPI.h"
#define SCK 12  // pins for sd card
#define MISO 13 // these values are set for the LILYGO TTGO T8 ESP32-S2 board
#define MOSI 11 // you may need to change these for other boards
#define SS 10
#define FILESYS SD
#else
#if USEFAT
#include "FFat.h"
#define FILESYS FFat
#else
#if USELFS
#include <LittleFS.h>
#define FILESYS LittleFS
#else
#include "SPIFFS.h"
#define FILESYS SPIFFS
#endif
#endif
#endif

//-------------------DEFAULT SETTINGS------------------//

                       // use config.ini [ true / false ]
#define USECONFIG true // this will allow you to change these settings below via the admin webpage.
                       // if you want to permanently use the values below then set this to false.

// create access point
boolean startAP = true;
String AP_SSID = "PS5_WEB_AP";
String AP_PASS = "password";
IPAddress Server_IP(10, 1, 1, 1);
IPAddress Subnet_Mask(255, 255, 255, 0);

// connect to wifi
boolean connectWifi = false;
String WIFI_SSID = "Home_WIFI";
String WIFI_PASS = "password";
String WIFI_HOSTNAME = "ps5.local";

// Auto Usb Wait(milliseconds)
int USB_WAIT = 10000;

// Displayed firmware version
String firmwareVer = "1.00";

//-----------------------------------------------------//

#include "Pages.h"
DNSServer dnsServer;
using namespace httpsserver;
SSLCert cert = SSLCert(crt_DER, crt_DER_len, key_DER, key_DER_len);
HTTPSServer secureServer = HTTPSServer(&cert);
HTTPServer insecureServer = HTTPServer();
boolean hasEnabled = false;
boolean isFormating = false;
boolean isRebooting = false;
long enTime = 0;
long bootTime = 0;
File upFile;
#if defined(CONFIG_IDF_TARGET_ESP32S2) | defined(CONFIG_IDF_TARGET_ESP32S3)
USBMSC dev;
#endif

/*
#if ARDUINO_USB_CDC_ON_BOOT
#define HWSerial Serial0
#define USBSerial Serial
#else
#define HWSerial Serial
#if defined(CONFIG_IDF_TARGET_ESP32S2) | defined(CONFIG_IDF_TARGET_ESP32S3)
USBCDC USBSerial;
#endif
#endif
*/

String split(String str, String from, String to)
{
  String tmpstr = str;
  tmpstr.toLowerCase();
  from.toLowerCase();
  to.toLowerCase();
  int pos1 = tmpstr.indexOf(from);
  int pos2 = tmpstr.indexOf(to, pos1 + from.length());   
  String retval = str.substring(pos1 + from.length() , pos2);
  return retval;
}


bool instr(String str, String search)
{
int result = str.indexOf(search);
if (result == -1)
{
  return false;
}
return true;
}


String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+" B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+" KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+" MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+" GB";
  }
}


String urlencode(String str)
{
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
      }
      yield();
    }
    encodedString.replace("%2E",".");
    return encodedString;
}


void handleFileMan(HTTPRequest *req, HTTPResponse *res)
{
  File dir = FILESYS.open("/");
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>File Manager</title><link rel=\"stylesheet\" href=\"style.css\"><style>body{overflow-y:auto;} th{border: 1px solid #dddddd; background-color:gray;padding: 8px;}</style><script>function statusDel(fname) {var answer = confirm(\"Are you sure you want to delete \" + fname + \" ?\");if (answer) {return true;} else { return false; }} </script></head><body><br><table id=filetable></table><script>var filelist = [";
  int fileCount = 0;
  while (dir)
  {
    File file = dir.openNextFile();
    if (!file)
    {
      dir.close();
      break;
    }
    String fname = String(file.name());
    if (fname.length() > 0 && !fname.equals("config.ini") && !file.isDirectory())
    {
      fileCount++;
      fname.replace("|", "%7C");
      fname.replace("\"", "%22");
      output += "\"" + fname + "|" + formatBytes(file.size()) + "\",";
    }
    file.close();
  }
  if (fileCount == 0)
  {
    output += "];</script><center>No files found<br>You can upload files using the <a href=\"/upload.html\" target=\"mframe\"><u>File Uploader</u></a> page.</center></p></body></html>";
  }
  else
  {
    output += "];var output = \"\";filelist.forEach(function(entry) {var splF = entry.split(\"|\"); output += \"<tr>\";output += \"<td><a href=\\\"\" +  splF[0] + \"\\\">\" + splF[0] + \"</a></td>\"; output += \"<td>\" + splF[1] + \"</td>\";output += \"<td><a href=\\\"/\" + splF[0] + \"\\\" download><button type=\\\"submit\\\">Download</button></a></td>\";output += \"<td><form action=\\\"/delete\\\" method=\\\"post\\\"><button type=\\\"submit\\\" name=\\\"file\\\" value=\\\"\" + splF[0] + \"\\\" onClick=\\\"return statusDel('\" + splF[0] + \"');\\\">Delete</button></form></td>\";output += \"</tr>\";}); document.getElementById(\"filetable\").innerHTML = \"<tr><th colspan='1'><center>File Name</center></th><th colspan='1'><center>File Size</center></th><th colspan='1'><center><a href='/dlall' target='mframe'><button type='submit'>Download All</button></a></center></th><th colspan='1'><center><a href='/format.html' target='mframe'><button type='submit'>Delete All</button></a></center></th></tr>\" + output;</script></body></html>";
  }
  res->setHeader("Content-Type", "text/html");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->setHeader("Content-Length", String(output.length()).c_str());
  res->println(output.c_str());
}


void handleDlFiles(HTTPRequest *req, HTTPResponse *res)
{
  File dir = FILESYS.open("/");
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>File Downloader</title><link rel=\"stylesheet\" href=\"style.css\"><style>body{overflow-y:auto;}</style><script type=\"text/javascript\" src=\"jzip.js\"></script><script>var filelist = [";
  int fileCount = 0;
  while (dir)
  {
    File file = dir.openNextFile();
    if (!file)
    {
      dir.close();
      break;
    }
    String fname = String(file.name());
    if (fname.length() > 0 && !fname.equals("config.ini") && !file.isDirectory())
    {
      fileCount++;
      fname.replace("\"", "%22");
      output += "\"" + fname + "\",";
    }
    file.close();
  }
  if (fileCount == 0)
  {
    output += "];</script></head><center>No files found to download<br>You can upload files using the <a href=\"/upload.html\" target=\"mframe\"><u>File Uploader</u></a> page.</center></p></body></html>";
  }
  else
  {
    output += "]; async function dlAll(){var zip = new JSZip();for (var i = 0; i < filelist.length; i++) {if (filelist[i] != ''){var xhr = new XMLHttpRequest();xhr.open('GET',filelist[i],false);xhr.overrideMimeType('text/plain; charset=x-user-defined'); xhr.onload = function(e) {if (this.status == 200) {zip.file(filelist[i], this.response,{binary: true});}};xhr.send();document.getElementById('fp').innerHTML = 'Adding: ' + filelist[i];await new Promise(r => setTimeout(r, 50));}}document.getElementById('gen').style.display = 'none';document.getElementById('comp').style.display = 'block';zip.generateAsync({type:'blob'}).then(function(content) {saveAs(content,'esp_files.zip');});}</script></head><body onload='setTimeout(dlAll,100);'><center><br><br><br><br><div id='gen' style='display:block;'><div id='loader'></div><br><br>Generating ZIP<br><p id='fp'></p></div><div id='comp' style='display:none;'><br><br><br><br>Complete<br><br>Downloading: esp_files.zip</div></center></body></html>";
  }
  res->setHeader("Content-Type", "text/html");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->setHeader("Content-Length", String(output.length()).c_str());
  res->println(output.c_str());
}


void handlePayloads(HTTPRequest *req, HTTPResponse *res)
{
  File dir = FILESYS.open("/");
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>ESP Server</title><link rel=\"stylesheet\" href=\"style.css\"><style>body { background-color: #1451AE; color: #ffffff; font-size: 14px; font-weight: bold; margin: 0 0 0 0.0; overflow-y:hidden; text-shadow: 3px 2px DodgerBlue;}</style><script>function setpayload(payload,title,waittime){ sessionStorage.setItem('payload', payload); sessionStorage.setItem('title', title); sessionStorage.setItem('waittime', waittime);  window.open('loader.html', '_self');}</script></head><body><center><h1>PS5 Payloads</h1>";
  int cntr = 0;
  int payloadCount = 0;
  if (USB_WAIT < 5000){USB_WAIT = 5000;} // correct unrealistic timing values
  if (USB_WAIT > 25000){USB_WAIT = 25000;}
  while (dir)
  {
    File file = dir.openNextFile();
    if (!file)
    {
      dir.close();
      break;
    }
    String fname = String(file.name());
    if (fname.endsWith(".gz"))
    {
      fname = fname.substring(0, fname.length() - 3);
    }
    if (fname.length() > 0 && fname.endsWith(".bin") && !file.isDirectory())
    {
      payloadCount++;
      String fnamev = fname;
      fnamev.replace(".bin", "");
      output += "<a onclick=\"setpayload('" + urlencode(fname) + "','" + fnamev + "','" + String(USB_WAIT) + "')\"><button class=\"btn\">" + fnamev + "</button></a>&nbsp;";
      cntr++;
      if (cntr == 4)
      {
        cntr = 0;
        output += "<p></p>";
      }
    }
    file.close();
  }
  if (payloadCount == 0)
  {
    output += "<msg>No .bin payloads found<br>You need to upload the payloads to the ESP32 board.<br>in the arduino ide select <b>Tools</b> &gt; <b>ESP32 Sketch Data Upload</b><br>or<br>Using a pc/laptop connect to <b>" + AP_SSID + "</b> and navigate to <a href=\"/admin.html\"><u>http://" + WIFI_HOSTNAME + "/admin.html</u></a> and upload the .bin payloads using the <b>File Uploader</b></msg></center></body></html>";
  }
  output += "</center></body></html>";
  res->setHeader("Content-Type", "text/html");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->setHeader("Content-Length", String(output.length()).c_str());
  res->println(output.c_str());
}


void handleDelete(HTTPRequest *req, HTTPResponse *res)
{
  if (req->getMethod() == "POST")
  {
    HTTPURLEncodedBodyParser parser(req);
    while (parser.nextField())
    {
      std::string name = parser.getFieldName();
      if (name == "file")
      {
        char buf[256];
        size_t readLength = parser.read((byte *)buf, 256);
        String path = std::string(buf, readLength).c_str();
        if (path.length() > 0)
        {
          if (FILESYS.exists("/" + path) && !path.equals("/") && !path.equals("config.ini"))
          {
            FILESYS.remove("/" + path);
          }
        }
      }
    }
  }
  res->setStatusCode(301);
  res->setStatusText("Moved Permanently");
  res->setHeader("Location", "/fileman.html");
}


void handleInfo(HTTPRequest *req, HTTPResponse *res)
{
  float flashFreq = (float)ESP.getFlashChipSpeed() / 1000.0 / 1000.0;
  FlashMode_t ideMode = ESP.getFlashChipMode();
  String mcuType = CONFIG_IDF_TARGET;
  mcuType.toUpperCase();
  String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>System Information</title><link rel=\"stylesheet\" href=\"style.css\"></head>";
  output += "<hr>###### Software ######<br><br>";
  output += "Firmware version " + firmwareVer + "<br>";
  output += "SDK version: " + String(ESP.getSdkVersion()) + "<br><hr>";
  output += "###### Board ######<br><br>";
  output += "MCU: " + mcuType + "<br>";
#if defined(USB_PRODUCT)
  output += "Board: " + String(USB_PRODUCT) + "<br>";
#endif
  output += "Chip Id: " + String(ESP.getChipModel()) + "<br>";
  output += "CPU frequency: " + String(ESP.getCpuFreqMHz()) + "MHz<br>";
  output += "Cores: " + String(ESP.getChipCores()) + "<br><hr>";
  output += "###### Flash chip information ######<br><br>";
  output += "Flash chip Id: " + String(ESP.getFlashChipMode()) + "<br>";
  output += "Estimated Flash size: " + formatBytes(ESP.getFlashChipSize()) + "<br>";
  output += "Flash frequency: " + String(flashFreq) + " MHz<br>";
  output += "Flash write mode: " + String((ideMode == FM_QIO ? "QIO" : ideMode == FM_QOUT ? "QOUT": ideMode == FM_DIO    ? "DIO": ideMode == FM_DOUT   ? "DOUT": "UNKNOWN")) + "<br><hr>";
  output += "###### Storage information ######<br><br>";
#if USESD
  output += "Storage Device: SD<br>";
#elif USEFAT
  output += "Filesystem: FatFs<br>";
#elif USELFS
  output += "Filesystem: LittleFS<br>";
#else
  output += "Filesystem: SPIFFS<br>";
#endif
  output += "Total Size: " + formatBytes(FILESYS.totalBytes()) + "<br>";
  output += "Used Space: " + formatBytes(FILESYS.usedBytes()) + "<br>";
  output += "Free Space: " + formatBytes(FILESYS.totalBytes() - FILESYS.usedBytes()) + "<br><hr>";
#if defined(CONFIG_IDF_TARGET_ESP32S2) | defined(CONFIG_IDF_TARGET_ESP32S3)
  if (ESP.getPsramSize() > 0)
  {
    output += "###### PSRam information ######<br><br>";
    output += "Psram Size: " + formatBytes(ESP.getPsramSize()) + "<br>";
    output += "Free psram: " + formatBytes(ESP.getFreePsram()) + "<br>";
    output += "Max alloc psram: " + formatBytes(ESP.getMaxAllocPsram()) + "<br><hr>";
  }
#endif
  output += "###### Ram information ######<br><br>";
  output += "Ram size: " + formatBytes(ESP.getHeapSize()) + "<br>";
  output += "Free ram: " + formatBytes(ESP.getFreeHeap()) + "<br>";
  output += "Max alloc ram: " + formatBytes(ESP.getMaxAllocHeap()) + "<br><hr>";
  output += "###### Sketch information ######<br><br>";
  output += "Sketch hash: " + ESP.getSketchMD5() + "<br>";
  output += "Sketch size: " + formatBytes(ESP.getSketchSize()) + "<br>";
  output += "Free space available: " + formatBytes(ESP.getFreeSketchSpace() - ESP.getSketchSize()) + "<br><hr>";
  output += "</html>";
  res->setHeader("Content-Type", "text/html");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->setHeader("Content-Length", String(output.length()).c_str());
  res->println(output.c_str());
}


void handleUpload(HTTPRequest *req, HTTPResponse *res)
{
  if (req->getMethod() == "POST")
  {
    HTTPBodyParser *parser;
    parser = new HTTPMultipartBodyParser(req);
    while (parser->nextField())
    {
      String filename = parser->getFieldFilename().c_str();
      if (!filename.startsWith("/"))
      {
        filename = "/" + filename;
      }
      File upFile = FILESYS.open(filename, "w");
      size_t fileLength = 0;
      while (!parser->endOfField())
      {
        byte buf[1024];
        size_t readLength = parser->read(buf, 1024);
        upFile.write(buf, readLength);
        fileLength += readLength;
      }
      upFile.close();
    }
    delete parser;
    res->setStatusCode(301);
    res->setStatusText("Moved Permanently");
    res->setHeader("Location", "/fileman.html");
  }
  else
  {
    res->setHeader("Content-Type", "text/html");
    res->setHeader("Content-Encoding", "gzip");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->write(upload_gz, sizeof(upload_gz));
  }
}


void handleReboot(HTTPRequest *req, HTTPResponse *res)
{
  if (req->getMethod() == "POST")
  {
    res->setHeader("Content-Type", "text/html");
    res->setHeader("Content-Encoding", "gzip");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->write(rebooting_gz, sizeof(rebooting_gz));
    delay(1000);
    isRebooting = true;
  }
  else
  {
    res->setHeader("Content-Type", "text/html");
    res->setHeader("Content-Encoding", "gzip");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->write(reboot_gz, sizeof(reboot_gz));
  }
}


void handleFormat(HTTPRequest *req, HTTPResponse *res)
{
  if (req->getMethod() == "POST")
  {
    res->setStatusCode(304);
    res->setStatusText("Not Modified");
    isFormating = true;
  }
  else
  {
    res->setHeader("Content-Type", "text/html");
    res->setHeader("Content-Encoding", "gzip");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->write(format_gz, sizeof(format_gz));
  }
}


#if USECONFIG
void handleConfig(HTTPRequest *req, HTTPResponse *res)
{
  if (req->getMethod() == "POST")
  {
    String tmpua = "false";
    String tmpcw = "false";
    String tmpip = Subnet_Mask.toString();
    String tmpsubn = Server_IP.toString();
    HTTPURLEncodedBodyParser parser(req);
    while (parser.nextField())
    {
      String param = parser.getFieldName().c_str();
      char buf[256];
      size_t readLength = parser.read((byte *)buf, 256);
      String value = std::string(buf, readLength).c_str();

      if (param.equals("ap_ssid"))
      {
        AP_SSID = value;
      }
      else if (param.equals("ap_pass"))
      {
        if (!value.equals("********"))
        {
          AP_PASS = value;
        }
      }
      else if (param.equals("web_ip"))
      {
        tmpip = value;
      }
      else if (param.equals("subnet"))
      {
        tmpsubn = value;
      }
      else if (param.equals("wifi_ssid"))
      {
        WIFI_SSID = value;
      }
      else if (param.equals("wifi_pass"))
      {
        if (!value.equals("********"))
        {
          WIFI_PASS = value;
        }
      }
      else if (param.equals("wifi_host"))
      {
        WIFI_HOSTNAME = value;
      }
      else if (param.equals("usbwait"))
      {
        USB_WAIT = value.toInt();
      }
      else if (param.equals("useap"))
      {
        tmpua = "true";
      }
      else if (param.equals("usewifi"))
      {
        tmpcw = "true";
      }
    }
    if (tmpua.equals("false") && tmpcw.equals("false"))
    {
      tmpua = "true";
    }
    File iniFile = FILESYS.open("/config.ini", "w");
    if (iniFile)
    {
      iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + tmpip + "\r\nSUBNET_MASK=" + tmpsubn + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nUSEAP=" + tmpua + "\r\nCONWIFI=" + tmpcw + "\r\nUSBWAIT=" + USB_WAIT + "\r\n");
      iniFile.close();
    }
    String output = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"8; url=/info.html\"><style type=\"text/css\">#loader {z-index: 1;width: 50px;height: 50px;margin: 0 0 0 0;border: 6px solid #f3f3f3;border-radius: 50%;border-top: 6px solid #3498db;width: 50px;height: 50px;-webkit-animation: spin 2s linear infinite;animation: spin 2s linear infinite; } @-webkit-keyframes spin {0%{-webkit-transform: rotate(0deg);}100%{-webkit-transform: rotate(360deg);}}@keyframes spin{0%{ transform: rotate(0deg);}100%{transform: rotate(360deg);}}body {background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;} #msgfmt {font-size: 16px; font-weight: normal;}#status {font-size: 16px; font-weight: normal;}</style></head><center><br><br><br><br><br><p id=\"status\"><div id='loader'></div><br>Config saved<br>Rebooting</p></center></html>";
    res->setHeader("Content-Type", "text/html");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->setHeader("Content-Length", String(output.length()).c_str());
    res->println(output.c_str());
    delay(1000);
    isRebooting = true;
  }
  else
  {
    String tmpUa = "";
    String tmpCw = "";
    if (startAP)
    {
      tmpUa = "checked";
    }
    if (connectWifi)
    {
      tmpCw = "checked";
    }
    String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Config Editor</title><style type=\"text/css\">body {background-color: #1451AE; color: #ffffff; font-size: 14px;font-weight: bold;margin: 0 0 0 0.0;padding: 0.4em 0.4em 0.4em 0.6em;}input[type=\"submit\"]:hover {background: #ffffff;color: green;}input[type=\"submit\"]:active{outline-color: green;color: green;background: #ffffff; }table {font-family: arial, sans-serif;border-collapse: collapse;}td {border: 1px solid #dddddd;text-align: left;padding: 8px;}th {border: 1px solid #dddddd; background-color:gray;text-align: center;padding: 8px;}</style></head><body><form action=\"/config.html\" method=\"post\"><center><table><tr><th colspan=\"2\"><center>Access Point</center></th></tr><tr><td>AP SSID:</td><td><input name=\"ap_ssid\" value=\"" + AP_SSID + "\"></td></tr><tr><td>AP PASSWORD:</td><td><input name=\"ap_pass\" value=\"********\"></td></tr><tr><td>AP IP:</td><td><input name=\"web_ip\" value=\"" + Server_IP.toString() + "\"></td></tr><tr><td>SUBNET MASK:</td><td><input name=\"subnet\" value=\"" + Subnet_Mask.toString() + "\"></td></tr><tr><td>START AP:</td><td><input type=\"checkbox\" name=\"useap\" " + tmpUa + "></td></tr><tr><th colspan=\"2\"><center>Wifi Connection</center></th></tr><tr><td>WIFI SSID:</td><td><input name=\"wifi_ssid\" value=\"" + WIFI_SSID + "\"></td></tr><tr><td>WIFI PASSWORD:</td><td><input name=\"wifi_pass\" value=\"********\"></td></tr><tr><td>WIFI HOSTNAME:</td><td><input name=\"wifi_host\" value=\"" + WIFI_HOSTNAME + "\"></td></tr><tr><td>CONNECT WIFI:</td><td><input type=\"checkbox\" name=\"usewifi\" " + tmpCw + "></tr><tr><th colspan=\"2\"><center>Auto USB Wait</center></th></tr><tr><td>WAIT TIME(ms):</td><td><input name=\"usbwait\" value=\"" + USB_WAIT + "\"></td></tr></table><br><input id=\"savecfg\" type=\"submit\" value=\"Save Config\"></center></form></body></html>";
    res->setHeader("Content-Type", "text/html");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->setHeader("Content-Length", String(output.length()).c_str());
    res->println(output.c_str());
  }
}
#endif


#if !USBCONTROL && defined(CONFIG_IDF_TARGET_ESP32) 
void handleCacheManifest(HTTPRequest *req, HTTPResponse *res) {
  #if !USBCONTROL
  String output = "CACHE MANIFEST\r\n";
  File dir = FILESYS.open("/");
  while(dir){
    File file = dir.openNextFile();
    if (!file)
    { 
      dir.close();
      break;
    }
    String fname = String(file.name());
    if (fname.length() > 0 && !fname.equals("config.ini") && !file.isDirectory())
    {
      if (fname.endsWith(".gz")) {
        fname = fname.substring(0, fname.length() - 3);
      }
     output += urlencode(fname) + "\r\n";
    }
     file.close();
  }
  if(!instr(output,"index.html\r\n"))
  {
    output += "index.html\r\n";
  }
  if(!instr(output,"menu.html\r\n"))
  {
    output += "menu.html\r\n";
  }
  if(!instr(output,"payloads.html\r\n"))
  {
    output += "payloads.html\r\n";
  }
  if(!instr(output,"style.css\r\n"))
  {
    output += "style.css\r\n";
  }
   res->setHeader("Content-Type", "text/cache-manifest");
   res->setStatusCode(200);
   res->setStatusText("OK");
   res->setHeader("Content-Length", String(output.length()).c_str());
   res->println(output.c_str());
  #else
   res->setStatusCode(404);
   res->setStatusText("Not Found");
   res->setHeader("Content-Type", "text/plain");
   res->println("Not Found");
  #endif
}
#endif


void sendwebmsg(HTTPRequest *req, HTTPResponse *res, String htmMsg)
{
   String output = "<!DOCTYPE html><html><head><style type=\"text/css\">body {background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>" + htmMsg + "</center></html>";
   res->setHeader("Content-Type", "text/html");
   res->setStatusCode(200);
   res->setStatusText("OK");
   res->setHeader("Content-Length", String(output.length()).c_str());
   res->println(output.c_str());
}


void handleFwUpdate(HTTPRequest *req, HTTPResponse *res) {
  if (req->getMethod() == "POST")
  {
    HTTPBodyParser *parser;
    parser = new HTTPMultipartBodyParser(req);
    while (parser->nextField())
    {
      String filename = parser->getFieldFilename().c_str();
      if (!filename.endsWith("fwupdate.bin")) {
        sendwebmsg(req, res, "Invalid update file: " + filename);
      }else{
      if(!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)){
        //Update.printError(Serial);
        //HWSerial.println("Update failed" + String(Update.errorString()));
        sendwebmsg(req, res, "Update Failed: " + String(Update.errorString()));
        return;
      }
      size_t fileLength = 0;
      while (!parser->endOfField())
      {
        byte buf[1024];
        size_t readLength = parser->read(buf, 1024);
        Update.write(buf, readLength);
        fileLength += readLength;
      }
      if(Update.end(true)){
        //HWSerial.printf("Update Success: %uB\n", fileLength);
        String output = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"8; url=/info.html\"><style type=\"text/css\">body {background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>Update Success, Rebooting.</center></html>";
        res->setHeader("Content-Type", "text/html");
        res->setStatusCode(200);
        res->setStatusText("OK");
        res->setHeader("Content-Length", String(output.length()).c_str());
        res->println(output.c_str());
        delay(1000);
        isRebooting = true;
      } else {
        //HWSerial.println("Update failed" + String(Update.errorString()));
        sendwebmsg(req, res, "Update Failed: " + String(Update.errorString()));
      }
    }
   }
    delete parser;
  }else{
    res->setHeader("Content-Type", "text/html");
    res->setHeader("Content-Encoding", "gzip");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->write(update_gz, sizeof(update_gz));
  }
}


void handleHTTP(HTTPRequest *req, HTTPResponse *res)
{
  String path = req->getRequestString().c_str();
  String dataType = "text/plain";
  if (path.endsWith("/"))
  {
    path += "index.html";
  }
  if (path.endsWith(".src"))
  {
    path = path.substring(0, path.lastIndexOf("."));
  }
  else if (path.endsWith(".html"))
  {
    dataType = "text/html";
  }
  else if (path.endsWith(".htm"))
  {
    dataType = "text/html";
  }
  else if (path.endsWith(".css"))
  {
    dataType = "text/css";
  }
  else if (path.endsWith(".js"))
  {
    dataType = "application/javascript";
  }
  else if (path.endsWith(".png"))
  {
    dataType = "image/png";
  }
  else if (path.endsWith(".gif"))
  {
    dataType = "image/gif";
  }
  else if (path.endsWith(".jpg"))
  {
    dataType = "image/jpeg";
  }
  else if (path.endsWith(".ico"))
  {
    dataType = "image/x-icon";
  }
  else if (path.endsWith(".xml"))
  {
    dataType = "text/xml";
  }
  else if (path.endsWith(".manifest"))
  {
    dataType = "text/cache-manifest";
  }
  else if (path.endsWith(".zip"))
  {
    dataType = "application/x-zip";
  }
  else if (path.endsWith(".gz"))
  {
    dataType = "application/x-gzip";
  }
  else if (path.endsWith(".bin"))
  {
    dataType = "application/octet-stream";
  }
  else if (path.endsWith(".json"))
  {
    dataType = "application/json";
  }

#if USECONFIG
  if (path.endsWith("/config.ini"))
  {
    res->setStatusCode(404);
    res->setStatusText("Not Found");
    res->setHeader("Content-Type", "text/plain");
    res->println("Not Found");
    return;
  }
#endif

  if (path.endsWith("/connecttest.txt"))
  {
    String output = "Microsoft Connect Test";
    res->setHeader("Content-Type", "text/plain");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->setHeader("Content-Length", String(output.length()).c_str());
    res->println(output.c_str());
    return;
  }

#if USECONFIG
  if (path.endsWith("/config.html"))
  {
    handleConfig(req, res);
    return;
  }
#endif

#if !USBCONTROL && defined(CONFIG_IDF_TARGET_ESP32)
  if (path.endsWith("/cache.manifest"))
  {
    handleCacheManifest(req, res);
    return;
  }
#endif

  if (path.endsWith("/fileman.html"))
  {
    handleFileMan(req, res);
    return;
  }

  if (path.endsWith("/dlall"))
  {
    handleDlFiles(req, res);
    return;
  }

  if (path.endsWith("/jzip.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->write(jzip_gz, sizeof(jzip_gz));
    return;
  }

  if (path.endsWith("/info.html"))
  {
    handleInfo(req, res);
    return;
  }

  if (path.endsWith("/update.html"))
  {
    handleFwUpdate(req, res);
    return;
  }

  if (path.endsWith("/admin.html"))
  {
    
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->write(admin_gz, sizeof(admin_gz));
    return;
  }

  if (path.endsWith("/reboot.html"))
  {
    handleReboot(req, res);
    return;
  }

  if (path.endsWith("/upload.html"))
  {
    handleUpload(req, res);
    return;
  }

  if (path.endsWith("/delete"))
  {
    handleDelete(req, res);
    return;
  }

  if (path.endsWith("/format.html"))
  {
    handleFormat(req, res);
    return;
  }

  if (path.endsWith("/usbon"))
  {
    enableUSB();
    String output = "ok";
    res->setHeader("Content-Type", "text/plain");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->setHeader("Content-Length", String(output.length()).c_str());
    res->println(output.c_str());
    return;
  }

  if (path.endsWith("/usboff"))
  {
    String output = "ok";
    res->setHeader("Content-Type", "text/plain");
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->setHeader("Content-Length", String(output.length()).c_str());
    res->println(output.c_str());
    disableUSB();
    return;
  }

  bool isGzip = false;
  bool fileFound = false;

  fileFound = FILESYS.exists(path + ".gz");
  if (!fileFound)
  {
    fileFound = FILESYS.exists(path);
  }
  else
  {
    path = path + ".gz";
    isGzip = true;
  }

  if (fileFound)
  {
    File dataFile = FILESYS.open(path, "r");
    int filesize = dataFile.size();
    res->setHeader("Content-Type", dataType.c_str());
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->setHeader("Content-Length", String(filesize).c_str());

    uint8_t buf[256];
    while (dataFile.available())
    {
      int n = dataFile.read(buf, sizeof(buf));
      for (int i = 0; i < n; i++)
      {
        res->write(buf[i]);
      }
    }
    dataFile.close();
  }
  else
  {

    if (path.equals("/index.html"))
    {
      res->setHeader("Content-Type", dataType.c_str());
      res->setHeader("Content-Encoding", "gzip");
      res->setStatusCode(200);
      res->setStatusText("OK");
      res->write(index_gz, sizeof(index_gz));
      return;
    }

    if (path.endsWith("/payloads.html"))
    {
      handlePayloads(req, res);
      return;
    }

#if !USBCONTROL && defined(CONFIG_IDF_TARGET_ESP32)
    if (path.endsWith("/menu.html"))
    {
      res->setHeader("Content-Type", dataType.c_str());
      res->setHeader("Content-Encoding", "gzip");
      res->setStatusCode(200);
      res->setStatusText("OK");
      res->write(menu_gz, sizeof(menu_gz));
      return;
    }
#endif

    if (path.endsWith("/style.css"))
    {
      res->setHeader("Content-Type", dataType.c_str());
      res->setHeader("Content-Encoding", "gzip");
      res->setStatusCode(200);
      res->setStatusText("OK");
      res->write(style_gz, sizeof(style_gz));
      return;
    }

    res->setStatusCode(404);
    res->setStatusText("Not Found");
    res->setHeader("Content-Type", "text/plain");
    res->println("Not Found");
  }
}


void handleHTTPS(HTTPRequest *req, HTTPResponse *res)
{
  std::string serverIP = Server_IP.toString().c_str();
  res->setStatusCode(301);
  res->setStatusText("Moved Permanently");
  res->setHeader("Location", "http://" + serverIP + "/index.html");
}


#if USECONFIG
void writeConfig()
{
  File iniFile = FILESYS.open("/config.ini", "w");
  if (iniFile)
  {
    String tmpua = "false";
    String tmpcw = "false";
    if (startAP)
    {
      tmpua = "true";
    }
    if (connectWifi)
    {
      tmpcw = "true";
    }
    iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + Server_IP.toString() + "\r\nSUBNET_MASK=" + Subnet_Mask.toString() + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nUSEAP=" + tmpua + "\r\nCONWIFI=" + tmpcw + "\r\nUSBWAIT=" + USB_WAIT + "\r\n");
    iniFile.close();
  }
}
#endif


void setup()
{
  //HWSerial.begin(115200);
  //HWSerial.println("Version: " + firmwareVer);
  //USBSerial.begin();

#if USBCONTROL && defined(CONFIG_IDF_TARGET_ESP32)
  pinMode(usbPin, OUTPUT);
  digitalWrite(usbPin, LOW);
#endif

#if USESD
  SPI.begin(SCK, MISO, MOSI, SS);
  if (FILESYS.begin(SS, SPI))
  {
#else
  if (FILESYS.begin(true))
  {
#endif

#if USECONFIG
    if (FILESYS.exists("/config.ini"))
    {
      File iniFile = FILESYS.open("/config.ini", "r");
      if (iniFile)
      {
        String iniData;
        while (iniFile.available())
        {
          char chnk = iniFile.read();
          iniData += chnk;
        }
        iniFile.close();

        if (instr(iniData, "SSID="))
        {
          AP_SSID = split(iniData, "SSID=", "\r\n");
          AP_SSID.trim();
        }

        if (instr(iniData, "PASSWORD="))
        {
          AP_PASS = split(iniData, "PASSWORD=", "\r\n");
          AP_PASS.trim();
        }

        if (instr(iniData, "WEBSERVER_IP="))
        {
          String strwIp = split(iniData, "WEBSERVER_IP=", "\r\n");
          strwIp.trim();
          Server_IP.fromString(strwIp);
        }

        if (instr(iniData, "SUBNET_MASK="))
        {
          String strsIp = split(iniData, "SUBNET_MASK=", "\r\n");
          strsIp.trim();
          Subnet_Mask.fromString(strsIp);
        }

        if (instr(iniData, "WIFI_SSID="))
        {
          WIFI_SSID = split(iniData, "WIFI_SSID=", "\r\n");
          WIFI_SSID.trim();
        }

        if (instr(iniData, "WIFI_PASS="))
        {
          WIFI_PASS = split(iniData, "WIFI_PASS=", "\r\n");
          WIFI_PASS.trim();
        }

        if (instr(iniData, "WIFI_HOST="))
        {
          WIFI_HOSTNAME = split(iniData, "WIFI_HOST=", "\r\n");
          WIFI_HOSTNAME.trim();
        }

        if (instr(iniData, "USEAP="))
        {
          String strua = split(iniData, "USEAP=", "\r\n");
          strua.trim();
          if (strua.equals("true"))
          {
            startAP = true;
          }
          else
          {
            startAP = false;
          }
        }

        if (instr(iniData, "CONWIFI="))
        {
          String strcw = split(iniData, "CONWIFI=", "\r\n");
          strcw.trim();
          if (strcw.equals("true"))
          {
            connectWifi = true;
          }
          else
          {
            connectWifi = false;
          }
        }
        if (instr(iniData, "USBWAIT="))
        {
          String strusw = split(iniData, "USBWAIT=", "\r\n");
          strusw.trim();
          USB_WAIT = strusw.toInt();
        }
      }
    }
    else
    {
      writeConfig();
    }
#endif
  }
  else
  {
    //HWSerial.println("Filesystem failed to mount");
  }

  if (startAP)
  {
    //HWSerial.println("SSID: " + AP_SSID);
    //HWSerial.println("Password: " + AP_PASS);
    //HWSerial.println("");
    //HWSerial.println("WEB Server IP: " + Server_IP.toString());
    //HWSerial.println("Subnet: " + Subnet_Mask.toString());
    //HWSerial.println("");
    WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
    WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str());
    //HWSerial.println("WIFI AP started");
    dnsServer.setTTL(30);
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    dnsServer.start(53, "*", Server_IP);
    //HWSerial.println("DNS server started");
    //HWSerial.println("DNS Server IP: " + Server_IP.toString());
  }

  if (connectWifi && WIFI_SSID.length() > 0 && WIFI_PASS.length() > 0)
  {
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.hostname(WIFI_HOSTNAME);
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    //HWSerial.println("WIFI connecting");
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      //HWSerial.println("Wifi failed to connect");
    }
    else
    {
      IPAddress LAN_IP = WiFi.localIP();
      if (LAN_IP)
      {
        //HWSerial.println("Wifi Connected");
        //HWSerial.println("WEB Server LAN IP: " + LAN_IP.toString());
        //HWSerial.println("WEB Server Hostname: " + WIFI_HOSTNAME);
        String mdnsHost = WIFI_HOSTNAME;
        mdnsHost.replace(".local", "");
        MDNS.begin(mdnsHost.c_str());
        if (!startAP)
        {
          dnsServer.setTTL(30);
          dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
          dnsServer.start(53, "*", LAN_IP);
          //HWSerial.println("DNS server started");
          //HWSerial.println("DNS Server IP: " + LAN_IP.toString());
        }
      }
    }
  }

  ResourceNode *nhttp = new ResourceNode("", "ANY", &handleHTTP);
  ResourceNode *nhttps = new ResourceNode("", "ANY", &handleHTTPS);

  secureServer.setDefaultNode(nhttps);
  insecureServer.setDefaultNode(nhttp);
  secureServer.start();
  insecureServer.start();

  if (secureServer.isRunning() && insecureServer.isRunning())
  {
    //HWSerial.println("HTTP Server started");
  }

  bootTime = millis();
}

#if defined(CONFIG_IDF_TARGET_ESP32S2) | defined(CONFIG_IDF_TARGET_ESP32S3)
static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
  if (lba > 4)
  {
    lba = 4;
  }
  memcpy(buffer, exfathax[lba] + offset, bufsize);
  return bufsize;
}

void enableUSB()
{
  dev.vendorID("PS5");
  dev.productID("ESP32 Server");
  dev.productRevision("1.0");
  dev.onRead(onRead);
  dev.mediaPresent(true);
  dev.begin(8192, 512);
  USB.begin();
  enTime = millis();
  hasEnabled = true;
}

void disableUSB()
{
  enTime = 0;
  hasEnabled = false;
  dev.end();
  isRebooting = true;
}
#else
void enableUSB()
{
  digitalWrite(usbPin, HIGH);
  enTime = millis();
  hasEnabled = true;
}

void disableUSB()
{
  enTime = 0;
  hasEnabled = false;
  digitalWrite(usbPin, LOW);
}
#endif


void loop()
{
  dnsServer.processNextRequest();
  secureServer.loop();
  insecureServer.loop();
  if (hasEnabled && millis() >= (enTime + 15000))
  {
    disableUSB();
  }
  if (isRebooting)
  {
    delay(2000);
    isRebooting = false;
    ESP.restart();
  }
#if !USESD
  if (isFormating)
  {
    //HWSerial.print("Formatting Storage");
    isFormating = false;
    FILESYS.end();
    FILESYS.format();
    FILESYS.begin(true);
    delay(1000);
#if USECONFIG
    writeConfig();
#endif
  }
#endif
}
