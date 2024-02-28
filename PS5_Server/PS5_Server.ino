#if defined(CONFIG_IDF_TARGET_ESP32S2) | defined(CONFIG_IDF_TARGET_ESP32S3)
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPSServer.hpp>
#include <SSLCert.hpp>
#include <HTTPRequest.hpp>
#include <HTTPResponse.hpp>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include "Adafruit_SPIFlash.h"
#include "USB.h"
#include "USBMSC.h"
#include "ff.h"
#include "ah.h"
#include "etahen.h"

Adafruit_FlashTransport_ESP32 fT;
Adafruit_SPIFlash flash(&fT);
FatVolume fatfs;
USBMSC MSC;
DNSServer dnsServer;
using namespace httpsserver;
SSLCert *cert;
HTTPSServer *secureServer;
WebServer server(80);
typedef File32 File;
#define firmwareVer "1.0"
#define PDESC "PS5-Server"
#define MDESC "PS5ESP32"
long btnFmt = 0;
String henBin = "";
#if defined(RGB_BUILTIN)
#define LEDPIN RGB_BUILTIN
#elif defined(LED_BUILTIN)
#define LEDPIN LED_BUILTIN
#else
#define LEDPIN 13 //15 //2  depends on the board used.
#endif


//-------------------DEFAULT SETTINGS------------------//

                        // use config.ini [ true / false ]
#define USECONFIG true  // this will allow you to change these settings in the config.ini file.
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

// load etaHen automatically
boolean autoHen = false;


//-----------------------------------------------------//




String split(String str, String from, String to) {
  String tmpstr = str;
  tmpstr.toLowerCase();
  from.toLowerCase();
  to.toLowerCase();
  int pos1 = tmpstr.indexOf(from);
  int pos2 = tmpstr.indexOf(to, pos1 + from.length());
  String retval = str.substring(pos1 + from.length(), pos2);
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


String getMimeType(String filename)
{
  if (filename.endsWith(".htm"))
    return "text/html";
  else if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  else if (filename.endsWith(".mjs"))
    return "application/javascript";
  else if (filename.endsWith(".png"))
    return "image/png";
  else if (filename.endsWith(".gif"))
    return "image/gif";
  else if (filename.endsWith(".jpg"))
    return "image/jpeg";
  else if (filename.endsWith(".ico"))
    return "image/x-icon";
  else if (filename.endsWith(".xml"))
    return "text/xml";
  else if (filename.endsWith(".pdf"))
    return "application/x-pdf";
  else if (filename.endsWith(".zip"))
    return "application/x-zip";
  else if (filename.endsWith(".gz"))
    return "application/x-gzip";
  else if (filename.endsWith(".bin"))
    return "application/octet-stream";
  else if (filename.endsWith(".manifest"))
    return "text/cache-manifest";
  return "text/plain";
}


void findHen(String directory)
{
  File file;
  char filename[50];
  File root = fatfs.open(directory, O_RDONLY);
  while (file.openNext(&root, O_RDONLY))
  {
    file.getName(filename, sizeof(filename));
    String fName = String(filename);
    if (!fName.startsWith("System Volume Information"))
    {
      if (file.isDir()){
        findHen(fName);
      }else{
        String lcfName = fName;
        lcfName.toLowerCase();
        if (lcfName.startsWith("etahen"))
        {
          henBin = fName;
        }
      }
    }
    file.close();
    if (henBin.length() > 0){break;}
  }
  root.close();
}


bool loadFromFlash(String path)
{
  if (path.endsWith("config.ini") || path.endsWith("pk.pem") || path.endsWith("cert.der"))
  {
    return false;
  }
  if (instr(path, "/update/") && instr(path, "/ps5/"))
  {
    server.send(200, "application/xml", "<?xml version=\"1.0\" ?><update_data_list><region id=\"us\"><force_update><system auto_update_version=\"00.00\" sdk_version=\"01.00.00.09-00.00.00.0.0\" upd_version=\"01.00.00.00\"/></force_update><system_pup auto_update_version=\"00.00\" label=\"20.02.02.20.00.07-00.00.00.0.0\" sdk_version=\"02.20.00.07-00.00.00.0.0\" upd_version=\"02.20.00.00\"><update_data update_type=\"full\"></update_data></system_pup></region></update_data_list>");
    return true;
  }
  if (path.endsWith("connecttest.txt"))
  {
    server.send(200, "text/plain", "Microsoft Connect Test");
    return true;
  }
  if (path.endsWith("/"))
  {
    path += "index.html";
  }


  if (autoHen)
  {
    if (path.endsWith("payload_map.js"))
    {
      henBin = "";
      findHen("/");
      if (henBin.length() >0)
      {
        if (henBin.endsWith(".gz"))
        {
          henBin.replace(".gz", "");
        }
      }
      else
      {
        henBin = "etahen.bin";
      }
      String output = "const payload_map =\r\n[";
      output += "{\r\n";
      output += "displayTitle: 'etaHEN',\r\n";
      output += "description: 'Runs With 3.xx and 4.xx. FPKG enabler For FW 3.xx / 4.03-4.51 Only.',\r\n";  
      output += "fileName: '" + henBin + "',\r\n";
      output += "author: 'LightningMods_, sleirsgoevy, ChendoChap, astrelsky, illusion',\r\n";
      output += "source: 'https://github.com/LightningMods/etaHEN',\r\n";
      output += "version: 'auto'\r\n}\r\n";
      output += "\r\n];";
      server.send(200, "application/javascript", output);
      return true;
    }

    if (path.endsWith("index.html"))
    {
        server.sendHeader("Content-Encoding", "gzip");
        server.send_P(200, "text/html", index_gz, sizeof(index_gz));
        return true;
    }
    if (path.endsWith("exploit.js"))
    {
        server.sendHeader("Content-Encoding", "gzip");
        server.send_P(200, "application/javascript", exploit_gz, sizeof(exploit_gz));
        return true;
    }
  }


  String mimeType = getMimeType(path);
  bool isGzip = false;
  File flashFile;
  flashFile = fatfs.open(path + ".gz", O_RDONLY);
  if (!flashFile)
  {
    flashFile = fatfs.open(path, O_RDONLY);
  }
  else
  {
    isGzip = true;
  }
  if (!flashFile)
  {
    return false;
  }
  if (isGzip)
  {
    server.sendHeader("Content-Encoding", "gzip");
  }
  if (server.streamFile(flashFile, mimeType) != flashFile.size())
  {
    //Serial.println("Sent less data than expected!");
  }
  flashFile.close();
  return true;
}


void handleNotFound()
{
  if (loadFromFlash(server.uri()))
  {
    return;
  }
  if (server.uri().endsWith("/") || server.uri().endsWith("index.html") || server.uri().endsWith("index.htm"))
  {
    server.send(200, "text/html", "<!DOCTYPE html><html><head><style>body{background-color: #2F3335;color: #ffffff;font-size: 18px; font-weight: bold; margin: 0 0 0 0.0; padding: 0.4em 0.4em 0.4em 0.6em;}</style></head><center><br><br><br><br><br><br>index.html not found<br>connect the esp to a pc and transfer the files to the storage</center></html>");
    return;
  }
  if (autoHen)
  {
    if (server.uri().endsWith("etahen.bin"))
    {
      server.sendHeader("Content-Encoding", "gzip");
      server.send_P(200, "application/octet-stream", etahen_gz, sizeof(etahen_gz));
      return;
    }
  }
  //Serial.println("Not Found: " + server.uri());
  server.send(404, "text/plain", "Not Found");
}


#if USECONFIG
void writeConfig() {
  File iniFile = fatfs.open("/config.ini", O_RDWR | O_CREAT);
  if (iniFile){
    String tmpcw = "false";
    String tmpah = "false";
    if (connectWifi){tmpcw = "true";}
    if (autoHen){tmpah = "true";}
    iniFile.print("NOTE: after making changes to this file you must reboot the esp board for the changes to take effect\r\n\r\nAP_SSID=" + AP_SSID + "\r\nAP_PASS=" + AP_PASS + "\r\nWEBSERVER_IP=" + Server_IP.toString() + "\r\nSUBNET_MASK=" + Subnet_Mask.toString() + "\r\nWIFI_SSID=" + WIFI_SSID + "\r\nWIFI_PASS=" + WIFI_PASS + "\r\nWIFI_HOST=" + WIFI_HOSTNAME + "\r\nCONWIFI=" + tmpcw + "\r\nAUTOHEN=" + tmpah + "\r\n");
    iniFile.close();
  }
}
#endif


void loadAP() {
  WiFi.softAPConfig(Server_IP, Server_IP, Subnet_Mask);
  WiFi.softAP(AP_SSID, AP_PASS);
  dnsServer.setTTL(30);
  dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
  dnsServer.start(53, "*", Server_IP);
}


void loadSTA() {
  WiFi.setHostname(WIFI_HOSTNAME.c_str());
  WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    loadAP();
  } else {
    IPAddress LAN_IP = WiFi.localIP();
    if (LAN_IP) {
      String mdnsHost = WIFI_HOSTNAME;
      mdnsHost.replace(".local", "");
      MDNS.begin(mdnsHost.c_str());
      dnsServer.setTTL(30);
      dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
      dnsServer.start(53, "*", LAN_IP);
    }
  }
}


void handleHTTPS(HTTPRequest *req, HTTPResponse *res)
{
  String path = req->getRequestString().c_str();
  if (instr(path, "/document/") && instr(path, "/ps5/"))
  {
    std::string serverHost = WIFI_HOSTNAME.c_str();
    res->setStatusCode(301);
    res->setStatusText("Moved Permanently");
    res->setHeader("Location", "http://" + serverHost + "/index.html");
    res->println("Moved Permanently");
  }
  else if (instr(path, "/update/") && instr(path, "/ps5/"))
  {
    res->setStatusCode(200);
    res->setStatusText("OK");
    res->setHeader("Content-Type", "application/xml");
    res->println("<?xml version=\"1.0\" ?><update_data_list><region id=\"us\"><force_update><system auto_update_version=\"00.00\" sdk_version=\"01.00.00.09-00.00.00.0.0\" upd_version=\"01.00.00.00\"/></force_update><system_pup auto_update_version=\"00.00\" label=\"20.02.02.20.00.07-00.00.00.0.0\" sdk_version=\"02.20.00.07-00.00.00.0.0\" upd_version=\"02.20.00.00\"><update_data update_type=\"full\"></update_data></system_pup></region></update_data_list>");
  }
  else
  {
    res->setStatusCode(404);
    res->setStatusText("Not Found");
    res->setHeader("Content-Type", "text/plain");
    res->println("Not Found");
  }
}


void setup()
{
  //Serial.begin(115200);
  //Serial.println("start");

  pinMode(LEDPIN,OUTPUT);
  digitalWrite(LEDPIN, LOW);
  
  flash.begin();
  if (!fatfs.begin(&flash))
  {
    //Serial.println("formatting");
    format_flash();
  }

#if USECONFIG
    if (fatfs.exists("/config.ini")) {
      File iniFile = fatfs.open("/config.ini", O_RDWR | O_CREAT);
      if (iniFile) {
        String iniData;
        while (iniFile.available()) {
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

        if (instr(iniData, "WEBSERVER_IP=")) {
          String strwIp = split(iniData, "WEBSERVER_IP=", "\r\n");
          strwIp.trim();
          Server_IP.fromString(strwIp);
        }

        if (instr(iniData, "SUBNET_MASK=")) {
          String strsIp = split(iniData, "SUBNET_MASK=", "\r\n");
          strsIp.trim();
          Subnet_Mask.fromString(strsIp);
        }

        if (instr(iniData, "WIFI_SSID=")) {
          WIFI_SSID = split(iniData, "WIFI_SSID=", "\r\n");
          WIFI_SSID.trim();
        }

        if (instr(iniData, "WIFI_PASS=")) {
          WIFI_PASS = split(iniData, "WIFI_PASS=", "\r\n");
          WIFI_PASS.trim();
        }

        if (instr(iniData, "WIFI_HOST=")) {
          WIFI_HOSTNAME = split(iniData, "WIFI_HOST=", "\r\n");
          WIFI_HOSTNAME.trim();
        }

        if (instr(iniData, "CONWIFI=")) {
          String strcw = split(iniData, "CONWIFI=", "\r\n");
          strcw.trim();
          strcw.toLowerCase();
          if (strcw.equals("true")) {
            connectWifi = true;
          } else {
            connectWifi = false;
          }
        }

        if (instr(iniData, "AUTOHEN=")) {
          String strah = split(iniData, "AUTOHEN=", "\r\n");
          strah.trim();
          strah.toLowerCase();
          if (strah.equals("true")) {
            autoHen = true;
          } else {
            autoHen = false;
          }
        }
      }
    } else {
      writeConfig();
    }
#endif

  if (connectWifi && WIFI_SSID.length() > 0 && WIFI_PASS.length() > 0) {
    loadSTA();
  } else {
    loadAP();
  }

  if (fatfs.exists("/pk.pem") && fatfs.exists("/cert.der"))
  {
    uint8_t *cBuffer;
    uint8_t *kBuffer;
    unsigned int clen = 0;
    unsigned int klen = 0;
    File certFile = fatfs.open("/cert.der", O_RDONLY);
    clen = certFile.size();
    cBuffer = (uint8_t *)malloc(clen);
    certFile.read(cBuffer, clen);
    certFile.close();
    File pkFile = fatfs.open("/pk.pem", O_RDONLY);
    klen = pkFile.size();
    kBuffer = (uint8_t *)malloc(klen);
    pkFile.read(kBuffer, klen);
    pkFile.close();
    cert = new SSLCert(cBuffer, clen, kBuffer, klen);
  }
  else
  {
    cert = new SSLCert();
    String keyInf = "CN=" + WIFI_HOSTNAME + ",O=" + PDESC + ",C=US";
    int createCertResult = createSelfSignedCert(*cert, KEYSIZE_1024, (std::string)keyInf.c_str(), "20240101000000", "20300101000000");
    if (createCertResult != 0)
    {
      //Serial.printf("Certificate failed, Error Code = 0x%02X\n", createCertResult);
    }
    else
    {
      //Serial.println("Certificate created");
      File pkFile = fatfs.open("/pk.pem", O_RDWR | O_CREAT);
      pkFile.write(cert->getPKData(), cert->getPKLength());
      pkFile.close();
      File certFile = fatfs.open("/cert.der", O_RDWR | O_CREAT);
      certFile.write(cert->getCertData(), cert->getCertLength());
      certFile.close();
    }
  }

  secureServer = new HTTPSServer(cert);
  ResourceNode *nhttps = new ResourceNode("", "ANY", &handleHTTPS);
  secureServer->setDefaultNode(nhttps);
  secureServer->start();

  server.onNotFound(handleNotFound);
  server.begin();

  MSC.vendorID(MDESC);
  MSC.productID(PDESC);
  MSC.productRevision(firmwareVer);
  MSC.onRead(onRead);
  MSC.onWrite(onWrite);
  MSC.mediaPresent(true);
  MSC.begin(flash.size() / 512, 512);
  USB.productName(PDESC);
  USB.manufacturerName(MDESC);
  USB.begin();
  
}


void loop()
{
  dnsServer.processNextRequest();
  server.handleClient();
  secureServer->loop();
  if (digitalRead(0) == LOW){
    if (btnFmt == 0){
       btnFmt = millis();
    }
    if (millis() >= (btnFmt + 10000)){ //hold boot button for 10 seconds to force format.
      btnFmt = 0;
      digitalWrite(LEDPIN, HIGH);
      //Serial.println("formatting");
      fatfs.end();
      MSC.end();
      format_flash();
      delay(2000);
      ESP.restart();
      return;
    }
    delay(200);
    digitalWrite(LEDPIN, HIGH);
    delay(200);
    digitalWrite(LEDPIN, LOW);
  }
  else{
    btnFmt = 0;
  }
}


static int32_t onWrite(uint32_t lba, uint32_t offset, uint8_t *buffer, uint32_t bufsize)
{
  return flash.writeBlocks(lba, buffer, bufsize / 512) ? bufsize : -1;
}


static int32_t onRead(uint32_t lba, uint32_t offset, void *buffer, uint32_t bufsize)
{
  return flash.readBlocks(lba, (uint8_t *)buffer, bufsize / 512) ? bufsize : -1;
}


void format_flash(void)
{
  uint8_t workbuf[4096];
  FATFS ecffs;
  FRESULT r = f_mkfs("", FM_FAT, 0, workbuf, sizeof(workbuf));
  if (r != FR_OK) {
    while(1) yield();
  }
  r = f_mount(&ecffs, "0:", 1);
  if (r != FR_OK) {
    while(1) yield();
  }
  r = f_setlabel(PDESC);
  if (r != FR_OK) {
    while(1) yield();
  }
  f_unmount("0:");
  flash.syncBlocks();
  if (!fatfs.begin(&flash)) {
    while(1) delay(1);
  }
}



extern "C"
{

DSTATUS disk_status(BYTE pdrv)
{
  (void) pdrv;
  return 0;
}

DSTATUS disk_initialize(BYTE pdrv)
{
  (void) pdrv;
  return 0;
}

DRESULT disk_read(BYTE pdrv,BYTE *buff, DWORD sector, UINT count)
{
  (void) pdrv;
  return flash.readBlocks(sector, buff, count) ? RES_OK : RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
  (void) pdrv;
  return flash.writeBlocks(sector, buff, count) ? RES_OK : RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
  (void) pdrv;
  switch ( cmd )
  {
    case CTRL_SYNC:
      flash.syncBlocks();
      return RES_OK;
    case GET_SECTOR_COUNT:
      *((DWORD*) buff) = flash.size()/512;
      return RES_OK;
    case GET_SECTOR_SIZE:
      *((WORD*) buff) = 512;
      return RES_OK;
    case GET_BLOCK_SIZE:
      *((DWORD*) buff) = 8;
      return RES_OK;
    default:
      return RES_PARERR;
  }
}
}
#else
#error "Selected board not supported"
#endif
