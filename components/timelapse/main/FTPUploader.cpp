#include <dirent.h>     // POSIX directory functions
#include <stdexcept>
#include <algorithm>    // sort

#include "FTPUploader.h"
#include "BBEsp32Lib.h"

using namespace std;
using namespace bbesp32lib;


FTPUploader::FTPUploader(String host, String user, String password, int port) :
  _host(host),
  _password(password),
  _port(port),
  _user(user)
  
{
  
}

bool FTPUploader::makeDir(const String& dir) {
  String dirSlashed = slash("", dir, "");
  ensureConnected();
  return _ftpClient->ftpClientMakeDir(dirSlashed.c_str(), _ftpClientNetBuf);
}
/**
   Upload a single file. Binary mode.
*/
void FTPUploader::uploadFile(String localFile, String remoteFile, bool overwrite) {

  ensureConnected();

  if (remoteFile == "") {
    remoteFile = localFile;
  }
  localFile = slash("", localFile, "");
  remoteFile = slash("", remoteFile, "");
  
  if (overwrite) {
    _ftpClient->ftpClientDelete(remoteFile.c_str(), _ftpClientNetBuf);
  }
  //debugV("ftpClientPutting: %s to %s", localFile.c_str(), remoteFile.c_str());

  if (!_ftpClient->ftpClientPut(localFile.c_str(), remoteFile.c_str(), FTP_CLIENT_BINARY, _ftpClientNetBuf)) {
    throwError("ftpClientPut");
  }
  //printf("Uploaded: %s to %s", localFile.c_str(), remoteFile.c_str());
  
}


void FTPUploader::ensureConnected () {
  const int maxy = 128;
  char buf[maxy];

  // todo: handle ftpserver reset connection

  // If connected already... return
  //
  if (_ftpClient && _ftpClientNetBuf) {
    if (_ftpClient->ftpClientGetSysType(buf, maxy, _ftpClientNetBuf)) {
      // OK
      return;
    } else {
      // 
      //_ftpClient->closeFtpClient(_ftpClientNetBuf);
      //debugE("Ftp disconnected. Reconnecting. WARNING: Probably a memory leak here.");
      _ftpClient->ftpClientQuit(_ftpClientNetBuf);
      _ftpClientNetBuf = 0;
      // _ftpClient should still be usable
    }
  }
  if (!_ftpClient) {
    _ftpClient = getFtpClient();
  }

  //
  // Try to connect
  //
  // Must manual lookup IP as ftpClient.c library doesn't work with hostname, only IP
  printf("WARN: no nslookup implemented. IPs only.");
  //IPAddress hostIP;
  /*

  if (!WiFi.hostByName(_host.c_str(), hostIP) ) {
    String e = "Couldn't lookup host: " + _host;
    //debugE("%s",e.c_str());
    throw runtime_error(e.c_str());
  }
  */
  if (!_ftpClient->ftpClientConnect(_host.c_str(), 21, &_ftpClientNetBuf)) {
    String s = "ftpClientConnect error: ";
    //s= s+hostIP.toString();
    //debugE("%s",s.c_str());
    throwError("ftpClientConnect error:");
  }
  if (!_ftpClient->ftpClientLogin(_user.c_str(), _password.c_str(), _ftpClientNetBuf)) {
    //debugE("ftpClientLogin error");
    throwError("ftpClientLogin error:");
  }
}

bool FTPUploader::testConnection() {
  try {
    ensureConnected();
    return true;
  } catch (const exception & e) {
    //debugW("testFTPConnection: %s", e.what());
    return false;
  }

}
void FTPUploader::throwError(String suffix) {

  char* err;
  String errStr;

  err = _ftpClient->ftpClientGetLastResponse(_ftpClientNetBuf);
  errStr = suffix + ":" + (err ? String(err) : "unknown");
  throw runtime_error(errStr.c_str());
}
