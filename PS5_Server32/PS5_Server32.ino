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
#include "jzip.h"
#include "etahen.h"
#include "offsets.h"
#include "exploit.h"
#include "module.h"



#if defined(CONFIG_IDF_TARGET_ESP32S2) | defined(CONFIG_IDF_TARGET_ESP32S3) | defined(CONFIG_IDF_TARGET_ESP32) 
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

// Displayed firmware version
String firmwareVer = "1.00";

//-----------------------------------------------------//

#include "Pages.h"
DNSServer dnsServer;
using namespace httpsserver;
SSLCert *cert;

HTTPSServer *secureServer;
HTTPServer insecureServer = HTTPServer();

boolean hasEnabled = false;
boolean isFormating = false;
boolean isRebooting = false;
File upFile;



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


void handlePayloads(HTTPRequest *req, HTTPResponse *res)
{
  String output = "const payload_map =\r\n[";
  output += "{\r\n";
  output += "displayTitle: 'etaHEN',\r\n"; //internal etahen bin
  output += "description: 'Runs With 3.xx and 4.xx. FPKG enabler For FW 3.xx / 4.03-4.51 Only.',\r\n";  
  output += "fileName: 'ethen.bin',\r\n";
  output += "author: 'LightningMods_, sleirsgoevy, ChendoChap, astrelsky, illusion',\r\n";
  output += "source: 'https://github.com/LightningMods/etaHEN',\r\n";
  output += "version: 'v1.3 beta'\r\n}\r\n";
  
  File dir = FILESYS.open("/");
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
    if (fname.length() > 0 && !file.isDirectory() && fname.endsWith(".bin") || fname.endsWith(".elf"))
    {
      String fnamev = fname;
      fnamev.replace(".bin", "");
      fnamev.replace(".elf", "");
      output += ",{\r\n";
      output += "displayTitle: '" + fnamev + "',\r\n";
      output += "description: '" + fname + "',\r\n";  
      output += "fileName: '" + fname + "',\r\n";
      output += "author: '',\r\n";
      output += "source: '',\r\n";
      output += "version: ''\r\n}\r\n";
    }
    file.close();
  }
  output += "\r\n];";
  res->setHeader("Content-Type", "application/javascript");
  res->setHeader("Content-Length", String(output.length()).c_str());
  res->println(output.c_str());
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
    if (fname.length() > 0 && !fname.equals("config.ini") && !file.isDirectory() && !fname.equals("cert.der") && !fname.equals("pk.pem"))
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
    if (fname.length() > 0 && !fname.equals("config.ini") && !file.isDirectory() && !fname.equals("cert.der") && !fname.equals("pk.pem"))
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
    res->write(upload_gz, sizeof(upload_gz));
  }
}


void handleReboot(HTTPRequest *req, HTTPResponse *res)
{
  if (req->getMethod() == "POST")
  {
    res->setHeader("Content-Type", "text/html");
    res->setHeader("Content-Encoding", "gzip");
    res->write(rebooting_gz, sizeof(rebooting_gz));
    delay(1000);
    isRebooting = true;
  }
  else
  {
    res->setHeader("Content-Type", "text/html");
    res->setHeader("Content-Encoding", "gzip");
    res->write(reboot_gz, sizeof(reboot_gz));
  }
}


void handleFormat(HTTPRequest *req, HTTPResponse *res)
{
#if USESD
    res->setStatusCode(304);
    res->setStatusText("Not Modified");
#else
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
    res->write(format_gz, sizeof(format_gz));
  }
#endif
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
      iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + tmpip + "\r\nSUBNET_MASK=" + tmpsubn + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nUSEAP=" + tmpua + "\r\nCONWIFI=" + tmpcw + "\r\n");
      iniFile.close();
    }
    String output = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"8; url=/info.html\"><style type=\"text/css\">#loader {z-index: 1;width: 50px;height: 50px;margin: 0 0 0 0;border: 6px solid #f3f3f3;border-radius: 50%;border-top: 6px solid #3498db;width: 50px;height: 50px;-webkit-animation: spin 2s linear infinite;animation: spin 2s linear infinite; } @-webkit-keyframes spin {0%{-webkit-transform: rotate(0deg);}100%{-webkit-transform: rotate(360deg);}}@keyframes spin{0%{ transform: rotate(0deg);}100%{transform: rotate(360deg);}}body {background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;} #msgfmt {font-size: 16px; font-weight: normal;}#status {font-size: 16px; font-weight: normal;}</style></head><center><br><br><br><br><br><p id=\"status\"><div id='loader'></div><br>Config saved<br>Rebooting</p></center></html>";
    res->setHeader("Content-Type", "text/html");
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
    String output = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>Config Editor</title><style type=\"text/css\">body {background-color: #1451AE; color: #ffffff; font-size: 14px;font-weight: bold;margin: 0 0 0 0.0;padding: 0.4em 0.4em 0.4em 0.6em;}input[type=\"submit\"]:hover {background: #ffffff;color: green;}input[type=\"submit\"]:active{outline-color: green;color: green;background: #ffffff; }table {font-family: arial, sans-serif;border-collapse: collapse;}td {border: 1px solid #dddddd;text-align: left;padding: 8px;}th {border: 1px solid #dddddd; background-color:gray;text-align: center;padding: 8px;}</style></head><body><form action=\"/config.html\" method=\"post\"><center><table><tr><th colspan=\"2\"><center>Access Point</center></th></tr><tr><td>AP SSID:</td><td><input name=\"ap_ssid\" value=\"" + AP_SSID + "\"></td></tr><tr><td>AP PASSWORD:</td><td><input name=\"ap_pass\" value=\"********\"></td></tr><tr><td>AP IP:</td><td><input name=\"web_ip\" value=\"" + Server_IP.toString() + "\"></td></tr><tr><td>SUBNET MASK:</td><td><input name=\"subnet\" value=\"" + Subnet_Mask.toString() + "\"></td></tr><tr><td>START AP:</td><td><input type=\"checkbox\" name=\"useap\" " + tmpUa + "></td></tr><tr><th colspan=\"2\"><center>Wifi Connection</center></th></tr><tr><td>WIFI SSID:</td><td><input name=\"wifi_ssid\" value=\"" + WIFI_SSID + "\"></td></tr><tr><td>WIFI PASSWORD:</td><td><input name=\"wifi_pass\" value=\"********\"></td></tr><tr><td>WIFI HOSTNAME:</td><td><input name=\"wifi_host\" value=\"" + WIFI_HOSTNAME + "\"></td></tr><tr><td>CONNECT WIFI:</td><td><input type=\"checkbox\" name=\"usewifi\" " + tmpCw + "></td></tr></table><br><input id=\"savecfg\" type=\"submit\" value=\"Save Config\"></center></form></body></html>";
    res->setHeader("Content-Type", "text/html");
    res->setHeader("Content-Length", String(output.length()).c_str());
    res->println(output.c_str());
  }
}
#endif


/*
void handleCacheManifest(HTTPRequest *req, HTTPResponse *res) {

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
    if (fname.length() > 0 && !fname.equals("config.ini") && !file.isDirectory() && !fname.equals("cert.der") && !fname.equals("pk.pem"))
    {
      if (fname.endsWith(".gz")) {
        fname = fname.substring(0, fname.length() - 3);
      }
     output += urlencode(fname) + "\r\n";
    }
     file.close();
  }
  if(!instr(output,"style.css\r\n"))
  {
    output += "style.css\r\n";
  }
   res->setHeader("Content-Type", "text/cache-manifest");
   res->setHeader("Content-Length", String(output.length()).c_str());
   res->println(output.c_str());
}
*/


void sendwebmsg(HTTPRequest *req, HTTPResponse *res, String htmMsg)
{
   String output = "<!DOCTYPE html><html><head><style type=\"text/css\">body {background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>" + htmMsg + "</center></html>";
   res->setHeader("Content-Type", "text/html");
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
        //Serial.println("Update failed" + String(Update.errorString()));
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
        //Serial.printf("Update Success: %uB\n", fileLength);
        String output = "<!DOCTYPE html><html><head><meta http-equiv=\"refresh\" content=\"8; url=/info.html\"><style type=\"text/css\">body {background-color: #1451AE; color: #ffffff; font-size: 20px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>Update Success, Rebooting.</center></html>";
        res->setHeader("Content-Type", "text/html");
        res->setHeader("Content-Length", String(output.length()).c_str());
        res->println(output.c_str());
        delay(1000);
        isRebooting = true;
      } else {
        //Serial.println("Update failed" + String(Update.errorString()));
        sendwebmsg(req, res, "Update Failed: " + String(Update.errorString()));
      }
    }
   }
    delete parser;
  }else{
    res->setHeader("Content-Type", "text/html");
    res->setHeader("Content-Encoding", "gzip");
    res->write(update_gz, sizeof(update_gz));
  }
}


/*
void sendPayload(HTTPRequest *req, HTTPResponse *res, String fileName, int port)
{
  //Serial.println("Sending: " + fileName);

  WiFiClient client;
  if (!client.connect(req->getClientIP(), port)) {
    delay(1000);
    res->setStatusCode(500);
    res->setStatusText("Internal Server Error");
    res->setHeader("Content-Type", "text/plain");
    res->println("Internal Server Error");
    //Serial.println("failed to connect");
  }
  else
  {
     delay(1000);
     File dataFile = FILESYS.open(fileName, "r");
     if (dataFile) {
       long filelen = dataFile.available();
       char* psdRamBuffer = (char*)ps_malloc(filelen);
       dataFile.readBytes(psdRamBuffer, filelen);
       dataFile.close(); 
       client.write(psdRamBuffer, filelen);
       free(psdRamBuffer);
  }
  else
  {
    //Serial.println("file not found");
  }
  client.stop();
  //Serial.println("Sent");
  res->setStatusCode(200);
  res->setStatusText("OK");
  res->setHeader("Content-Type", "text/plain");
  res->println("OK");
  }
}
*/

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
  if (path.endsWith("/config.ini") || path.endsWith("/cert.der") || path.endsWith("/pk.pem"))
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


  if (path.endsWith("/index.html"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(index_gz, sizeof(index_gz));
    return;
  }

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
    res->write(jzip_gz, sizeof(jzip_gz));
    return;
  }


  if (path.endsWith("/3.00.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(o3_00_gz, sizeof(o3_00_gz));
    return;
  }

  if (path.endsWith("/3.10.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(o3_10_gz, sizeof(o3_10_gz));
    return;
  }

    if (path.endsWith("/3.20.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(o3_20_gz, sizeof(o3_20_gz));
    return;
  }

    if (path.endsWith("/3.21.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(o3_21_gz, sizeof(o3_21_gz));
    return;
  }

    if (path.endsWith("/4.00.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(o4_00_gz, sizeof(o4_00_gz));
    return;
  }

    if (path.endsWith("/4.02.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(o4_02_gz, sizeof(o4_02_gz));
    return;
  }

    if (path.endsWith("/4.03.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(o4_03_gz, sizeof(o4_03_gz));
    return;
  }

    if (path.endsWith("/4.50.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(o4_50_gz, sizeof(o4_50_gz));
    return;
  }

    if (path.endsWith("/4.51.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(o4_51_gz, sizeof(o4_51_gz));
    return;
  }

  if (path.endsWith("/module/int64.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(int64m_gz, sizeof(int64m_gz));
    return;
  }

  if (path.endsWith("/module/utils.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(utils_gz, sizeof(utils_gz));
    return;
  }

  if (path.endsWith("/module/constants.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(constants_gz, sizeof(constants_gz));
    return;
  }

  if (path.endsWith("/module/mem.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(mem_gz, sizeof(mem_gz));
    return;
  }

  if (path.endsWith("/module/memtools.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(memtools_gz, sizeof(memtools_gz));
    return;
  }

  if (path.endsWith("/module/rw.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(rw_gz, sizeof(rw_gz));
    return;
  }

  if (path.endsWith("/custom_host_stuff.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(custom_host_stuff_gz, sizeof(custom_host_stuff_gz));
    return;
  }

  if (path.endsWith("/exploit.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(exploit_gz, sizeof(exploit_gz));
    return;
  }

  if (path.endsWith("/int64.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(int64_gz, sizeof(int64_gz));
    return;
  }

  if (path.endsWith("/main.css"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(main_gz, sizeof(main_gz));
    return;
  }

  if (path.endsWith("/rop.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(rop_gz, sizeof(rop_gz));
    return;
  }

  if (path.endsWith("/rop_slave.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(rop_slave_gz, sizeof(rop_slave_gz));
    return;
  }

  if (path.endsWith("/webkit_psfree.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(webkit_psfree_gz, sizeof(webkit_psfree_gz));
    return;
  }


  if (path.endsWith("/webkit_fontface.js"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(webkit_fontface_gz, sizeof(webkit_fontface_gz));
    return;
  }

  
  if (path.endsWith("/ethen.bin"))
  {
    res->setHeader("Content-Type", dataType.c_str());
    res->setHeader("Content-Encoding", "gzip");
    res->write(etahen, sizeof(etahen));
    return;
  }


  if (path.endsWith("/payload_map.js"))
  {
    handlePayloads(req, res);
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
/*
  if (path.endsWith(".bin"))
  {
    sendPayload(req, res, path, 9020);
    return;
  }

  if (path.endsWith(".elf"))
  {
    sendPayload(req, res, path, 9027);
    return;
  }
*/
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
    res->setHeader("Content-Length", String(filesize).c_str());
    if(isGzip){
      res->setHeader("Content-Encoding", "gzip");
    }
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

    if (path.endsWith("/style.css"))
    {
      res->setHeader("Content-Type", dataType.c_str());
      res->setHeader("Content-Encoding", "gzip");
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
  String path = req->getRequestString().c_str();
  if (instr(path, "/document/") && instr(path, "/ps5/")) {
    std::string serverHost = WIFI_HOSTNAME.c_str();
    res->setStatusCode(301);
    res->setStatusText("Moved Permanently");
    res->setHeader("Location", "http://" + serverHost + "/index.html");
    res->println("Moved Permanently");
  }
  else if (instr(path, "/update/") && instr(path, "/ps5/")) {
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->setHeader("Content-Type", "application/xml");
    res->println("<?xml version=\"1.0\" ?><update_data_list><region id=\"us\"><force_update><system auto_update_version=\"00.00\" sdk_version=\"01.00.00.09-00.00.00.0.0\" upd_version=\"01.00.00.00\"/></force_update><system_pup auto_update_version=\"00.00\" label=\"20.02.02.20.00.07-00.00.00.0.0\" sdk_version=\"02.20.00.07-00.00.00.0.0\" upd_version=\"02.20.00.00\"><update_data update_type=\"full\"></update_data></system_pup></region></update_data_list>");
  }
  else{
    res->setStatusCode(404);
    res->setStatusText("Not Found");
    res->setHeader("Content-Type", "text/plain");
    res->println("Not Found");
  }
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
    iniFile.print("\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + Server_IP.toString() + "\r\nSUBNET_MASK=" + Subnet_Mask.toString() + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nUSEAP=" + tmpua + "\r\nCONWIFI=" + tmpcw + "\r\n");
    iniFile.close();
  }
}
#endif


void setup()
{
  //Serial.begin(115200);
  //Serial.println("Version: " + firmwareVer);
  
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

        if (instr(iniData, "AP_SSID=")) {
          AP_SSID = split(iniData, "AP_SSID=", "\r\n");
          AP_SSID.trim();
        }

        if (instr(iniData, "AP_PASS=")) {
          AP_PASS = split(iniData, "AP_PASS=", "\r\n");
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
    //Serial.println("Filesystem failed to mount");
  }

  if (startAP)
  {
    //Serial.println("SSID: " + AP_SSID);
    //Serial.println("Password: " + AP_PASS);
    //Serial.println("");
    //Serial.println("WEB Server IP: " + Server_IP.toString());
    //Serial.println("Subnet: " + Subnet_Mask.toString());
    //Serial.println("");
    WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
    WiFi.softAP(AP_SSID.c_str(), AP_PASS.c_str());
    //Serial.println("WIFI AP started");
    dnsServer.setTTL(30);
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    dnsServer.start(53, "*", Server_IP);
    //Serial.println("DNS server started");
    //Serial.println("DNS Server IP: " + Server_IP.toString());
  }

  if (connectWifi && WIFI_SSID.length() > 0 && WIFI_PASS.length() > 0)
  {
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    WiFi.hostname(WIFI_HOSTNAME);
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    //Serial.println("WIFI connecting");
    if (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      //Serial.println("Wifi failed to connect");
    }
    else
    {
      IPAddress LAN_IP = WiFi.localIP();
      if (LAN_IP)
      {
        //Serial.println("Wifi Connected");
        //Serial.println("WEB Server LAN IP: " + LAN_IP.toString());
        //Serial.println("WEB Server Hostname: " + WIFI_HOSTNAME);
        String mdnsHost = WIFI_HOSTNAME;
        mdnsHost.replace(".local", "");
        MDNS.begin(mdnsHost.c_str());
        if (!startAP)
        {
          dnsServer.setTTL(30);
          dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
          dnsServer.start(53, "*", LAN_IP);
          //Serial.println("DNS server started");
          //Serial.println("DNS Server IP: " + LAN_IP.toString());
        }
      }
    }
  }


  if (FILESYS.exists("/pk.pem") && FILESYS.exists("/cert.der"))
  {
    uint8_t* cBuffer;
    uint8_t* kBuffer;
    unsigned int clen = 0;
    unsigned int klen = 0;
    File certFile = FILESYS.open("/cert.der", "r");
    clen = certFile.size();
    cBuffer = (uint8_t*)malloc(clen);
    certFile.read(cBuffer, clen);
    certFile.close();
    File pkFile = FILESYS.open("/pk.pem", "r");
    klen = pkFile.size();
    kBuffer = (uint8_t*)malloc(klen);
    pkFile.read(kBuffer, klen);
    pkFile.close();
    cert = new SSLCert(cBuffer, clen, kBuffer, klen);
  }else{
    cert = new SSLCert();
    String keyInf = "CN=" + WIFI_HOSTNAME + ",O=Esp32_Server,C=US";
    int createCertResult = createSelfSignedCert(*cert, KEYSIZE_1024, (std::string)keyInf.c_str(), "20190101000000", "20300101000000");
    if (createCertResult != 0) {
      //Serial.printf("Certificate failed, Error Code = 0x%02X\n", createCertResult);
    }else{
      //Serial.println("Certificate created");
      File pkFile = FILESYS.open("/pk.pem", "w");
      pkFile.write( cert->getPKData(), cert->getPKLength());
      pkFile.close();
      File certFile = FILESYS.open("/cert.der", "w");
      certFile.write(cert->getCertData(), cert->getCertLength());
      certFile.close();
    }
  }

  secureServer = new HTTPSServer(cert);

  ResourceNode *nhttp = new ResourceNode("", "ANY", &handleHTTP);
  ResourceNode *nhttps = new ResourceNode("", "ANY", &handleHTTPS);

  secureServer->setDefaultNode(nhttps);
  secureServer->start();
  
  insecureServer.setDefaultNode(nhttp);
  insecureServer.start();

  if (secureServer->isRunning() && insecureServer.isRunning())
  {
    //Serial.println("HTTP Server started");
  }

}



void loop()
{

  dnsServer.processNextRequest();
  secureServer->loop();
  insecureServer.loop();

  if (isRebooting)
  {
    delay(2000);
    isRebooting = false;
    ESP.restart();
  }
#if !USESD
  if (isFormating)
  {
    //Serial.print("Formatting Storage");
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
