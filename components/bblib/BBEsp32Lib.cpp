#include "Arduino.h"
  
#include "BBEsp32Lib.h"
#include "driver/sdmmc_host.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include <dirent.h>
#include <errno.h>
#include <freertos/task.h>
#include <rom/rtc.h>
#include <stdexcept>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/unistd.h>
#include <time.h>

// MACROS to copy to your cpp
#define TRACE printf("TRACE %s:%d\n", __FILE__, __LINE__);
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)                                \
  (byte & 0x80 ? '1' : '0'), (byte & 0x40 ? '1' : '0'),     \
      (byte & 0x20 ? '1' : '0'), (byte & 0x10 ? '1' : '0'), \
      (byte & 0x08 ? '1' : '0'), (byte & 0x04 ? '1' : '0'), \
      (byte & 0x02 ? '1' : '0'), (byte & 0x01 ? '1' : '0')

using namespace std;

static const char *TAG = "bbesp32lib";
#undef LOG_LOCAL_LEVEL
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

namespace bbesp32lib {

BlinkClass Blink;
LogFileClass LogFile;

void taskState(TaskHandle_t task) {
  // eTaskState   ts = eTaskGetState(task);
  //  debugI("%s: state:%l hwm:%l rtc:%l", ts.pcTaskName, (long)
  //  ts.eCurrentState,
  //       (long) ts.usStackHighWaterMark, (long)ts.ulRunTimeCounter);
}

/**
   Return "startWith + from + endWith" without any repeated substrings
*/
String slash(const String &startWith, const String &from,
             const String &endWith) {

  String r = from;

  if (r.startsWith(ps)) {
    r = r.substring(ps.length());
    if (r.startsWith(ps)) {
      r = r.substring(ps.length());
    }
  }

  if (r.endsWith(ps)) {
    r = r.substring(0, r.length() - ps.length());
    if (r.endsWith(ps)) {
      r = r.substring(0, r.length() - ps.length());
    }
  }
  return startWith + r + endWith;
}

String slash(char startWith, const String &from) {
  String r = from;

  if (r.startsWith(ps)) {
    r = r.substring(ps.length());
    if (r.startsWith(ps)) {
      r = r.substring(ps.length());
    }
  }

  return String(startWith) + r;
}

String slash(const String &from, char endWith) {
  String r = from;

  if (r.endsWith(ps)) {
    r = r.substring(0, r.length() - ps.length());
    if (r.endsWith(ps)) {
      r = r.substring(0, r.length() - ps.length());
    }
  }
  return r + String(endWith);
}

/**
    Use posix functions - much faster than Arduino File class for listDir
*/
std::vector<String> ls(const String &localDir, int maxFiles,
                       const String &endingWith) {

  std::vector<String> r;
  const int isFile = 1; // Not portable!
  DIR *folder;
  struct dirent *entry;
  int files = 0;
  int niles = 0;
  String localDirPosix;
  String filename;
  String localDirSlashed;

  if (localDir != "") {
    localDirSlashed = slash("", localDir, "/");
  }
  localDirPosix = slash("", localDir, "");
  folder = opendir(localDirPosix.c_str());

  if (!folder) {
    printf("ls: couldn't open directory: %s", localDirPosix.c_str());
    return r;
  }
  /*
    Serial.println("------BEFORE---------");
    for (size_t i = 0; i < r.size(); ++i)  {
    Serial.println(r[i]);
    }
    Serial.println("-------------------");
    Serial.println(maxFiles);
    Serial.println(files);
  */

  while ((entry = readdir(folder))) {
    if (entry->d_type == isFile && entry->d_name) {
      filename = String(entry->d_name);
      if (filename != "" && filename.endsWith(endingWith)) {
        files++;
        if (files < maxFiles) {
          // Serial.println("<");Serial.println(filename);
          r.push_back(filename);
        } else if (files == maxFiles) {
          Serial.println("==");
          Serial.println(filename);
          std::sort(r.begin(), r.end());
          r.push_back(filename);
        } else if (files > maxFiles) {
          std::vector<String>::iterator i;
          for (i = r.begin(); i != r.end() && (*i) < filename; ++i) {
            // Serial.print(".");
          }

          if (i != r.end()) {
            r.insert(i, filename); // insert before iterator it
            r.pop_back();
          }
        } else {
          assert(0);
        }
      } else {
        niles++;
      }
    }
  }
  return r;
}

/**
    Use posix functions - much faster than Arduino File class for listDir
*/
bool lsStats(const String &localDir, int *nFiles, String *firstFile,
             String *lastFile, const String &endingWith) {

  const int isFile = 1; // Not portable!
  DIR *folder;
  struct dirent *entry;

  String filename;
  String localDirSlashed;

  if (localDir != "") {
    localDirSlashed = slash("", localDir, "/");
  }

  folder = opendir(localDirSlashed.c_str());

  if (!folder) {
    printf("ls: couldn't open directory: %s", localDirSlashed.c_str());
    return false;
  }
  *nFiles = 0;
  while ((entry = readdir(folder))) {
    if (entry->d_type == isFile && entry->d_name) {
      filename = String(entry->d_name);
      if (filename != "" && filename.endsWith(endingWith)) {
        (*nFiles)++;
        if (*lastFile == "" || filename > *lastFile) {
          *lastFile = filename;
        }
        if (*firstFile == "" || filename < *firstFile) {
          *firstFile = filename;
        }
      }
    }
  }
  closedir(folder);
  return true;
}

// function to round the number
unsigned long mround_ul(unsigned long n, int m) {
  // Smaller multiple
  int a = (n / m) * m;
  // Larger multiple
  int b = a + m;
  // Return of closest of two
  return (n - a > b - n) ? b : a;
}

// function to round the number
time_t mround_time_t(time_t n, int m) {
  // Smaller multiple
  int a = (n / m) * m;
  // Larger multiple
  int b = a + m;
  // Return of closest of two
  return (n - a > b - n) ? b : a;
}

uint64_t uSecToNextMultiple(time_t secondsMultiple) {
  /*
  struct timeval {
    time_t      tv_sec;     // sec
    suseconds_t tv_usec;    // micro 1e6
 */
  struct timeval nextWake;
  struct timeval now;
  struct timeval sleepDuration;

  if (gettimeofday(&now, NULL) != 0) {
    throw runtime_error("gettimeofday failed");
  }

  nextWake.tv_usec = 0;
  nextWake.tv_sec =
      ((int)(now.tv_sec / secondsMultiple)) * secondsMultiple + secondsMultiple;
  //       A     -    B  =      C
  timersub(&nextWake, &now, &sleepDuration); // returns void, normalises

  ESP_LOGV(TAG, "now = %ld : %ld", now.tv_sec, now.tv_usec);
  ESP_LOGV(TAG, "nextWake = %ld : %ld", nextWake.tv_sec, nextWake.tv_usec);
  ESP_LOGV(TAG, "sleepDuration = %ld : %ld", sleepDuration.tv_sec,
           sleepDuration.tv_usec);

  return ((uint64_t)(sleepDuration.tv_sec * (uint64_t)1000000) +
          (uint64_t)sleepDuration.tv_usec);
}

void BlinkTask(void *blinkClassPtr) {
  TickType_t xLastWakeTime;
  BlinkClass *b = (BlinkClass *)blinkClassPtr;
  xLastWakeTime = xTaskGetTickCount();

  ESP_LOGV(TAG, "Starting blink _onMs: %d _offMs: %d state:%d", b->_onMs,
           b->_offMs, b->_on);
  for (;;) {
    // ESP_LOGI(TAG,"blink _onMs: %d _offMs: %d
    // state:%d",b->_onMs,b->_offMs,b->_on);

    if (b->_onMs > 0 && b->_offMs > 0) {
      gpio_set_level(b->_pin, b->_on ? BlinkClass::ON : BlinkClass::OFF);
      // todo redesign blink: needs semaphore protection to avoid change in b->on here!
      vTaskDelayUntil(&xLastWakeTime,
                      pdMS_TO_TICKS(b->_on ? b->_onMs : b->_offMs));
      b->_on = !b->_on;
    } else {
      vTaskDelay(1000);
    }
  }
}

BlinkClass::BlinkClass() {}

void BlinkClass::begin(gpio_num_t pin) {
  _pin = pin;
  gpio_pad_select_gpio(_pin);
  gpio_set_direction(_pin, GPIO_MODE_OUTPUT);
  gpio_set_level(_pin, OFF);
  xTaskCreatePinnedToCore(BlinkTask, "BlinkTask", 10000, (void *)this,
                          2 /* priority*/, NULL /* task handle */,
                          1 /* core */);
}

void BlinkClass::end() {
  _onMs = 0;
  _offMs = 0;
  gpio_set_level(_pin, OFF);
}

void BlinkClass::set(float freqHz, float onMs, bool heartbeat) {
  float periodMs = 1.0f / freqHz * 1000;
  _onMs = onMs;
  _offMs = periodMs - onMs;
}

void BlinkClass::flash(Duration d) {
  int saveOnMs = _onMs;
  _onMs = 0;
  gpio_set_level(_pin, BlinkClass::ON);
  vTaskDelay(pdMS_TO_TICKS(d));
  gpio_set_level(_pin, BlinkClass::OFF);
  _onMs = saveOnMs;
}

void BlinkClass::flash(int n, Speed s, Duration d) {
  int saveOnMs = _onMs;
  float periodMs = 1.0f / s * 1000.0f;
  int onMs = periodMs * .5;
  int offMs = periodMs - onMs;
  _onMs = 0;

  for (int i = 0; i < n; i++) {
    vTaskDelay(pdMS_TO_TICKS(onMs));
    gpio_set_level(_pin, BlinkClass::ON);
    vTaskDelay(pdMS_TO_TICKS(offMs));
    gpio_set_level(_pin, BlinkClass::OFF);
  }
  _onMs = saveOnMs;
}

int LogFileClass::vprintfHook(const char *fmt, va_list args) {
  return LogFile.vprintf(fmt, args);
}

LogFileClass::LogFileClass() {
  setFilename("log.txt");
}

LogFileClass::LogFileClass(const char *filename) {
  setFilename(filename);
}

String LogFileClass::getFilename() {
  return _filename;
}

void LogFileClass::setFilename(const char *filename) {
  _filename = filename;
  lastError=0;
  if (_filePtr) {
    fclose(_filePtr);
    _filePtr = NULL;
  }
}

bool LogFileClass::init() {
  if (_filePtr) {
    return true;
  } else {
    _filePtr = fopen(_filename.c_str(), "a"); // creates file
    fprintf(_filePtr,"Hi there!\n");
    fprintf(_filePtr,"Hi there!\n");
    fprintf(_filePtr,"Hi there!\n");
    if (lastError==0 && errno) {
      lastError=errno;
      printf("Error initting log: %s: %s\n", _filename.c_str(), strerror(lastError));
    }
    return (_filePtr != NULL);
  }
}

LogFileClass::~LogFileClass() {
  if (_filePtr) {
    fclose(_filePtr);
    _filePtr = NULL;
  }
}

void LogFileClass::captureESPLog(bool alsoWriteToStdout) {
  //vprintf_like_t oldFunction;
  //oldFunction =
  esp_log_set_vprintf(&LogFileClass::vprintfHook);
  vprintfCopyToStdout = alsoWriteToStdout;
}

int LogFileClass::vprintf(const char *fmt, va_list args) {

  int written = 0;

  if (init()) {
    // goto end of file
    if (fseek(_filePtr, 0, SEEK_END)) {
      perror("fseek() failed");
    }
    if (timeIsValid()) {
      fprintf(_filePtr, "%s:", timestamp().c_str());
    } else {
      fprintf(_filePtr, "%dms:", (int)esp_timer_get_time() / 1000);
    }

    written = vfprintf(_filePtr, fmt, args);
    fputs("\n", _filePtr);
  }

  if (LogFile.vprintfCopyToStdout) {
    vfprintf(stdout, fmt, args);
  }
  return written;
}

void LogFileClass::print(const String &message) {
  String t;

  time_t now;
  struct tm timeinfo;

  time(&now);
  localtime_r(&now, &timeinfo);

  if (timeinfo.tm_year > 2000) {
    t = timestamp();
  } else {
    t = String(millis()) + String("ms");
  }

  if (init()) {
    // goto end of file
    if (fseek(_filePtr, 0, SEEK_END)) {
      perror("fseek() failed");
    }

    fprintf(_filePtr, "%s: ", t.c_str());
    fputs(message.c_str(), _filePtr);
    fputs("\n", _filePtr);
  }
}

void LogFileClass::read(bool header) {

  const int MAXCHAR = 256;
  char str[MAXCHAR];

  if (header) {
    printf("----- START LOG ------\n");
    printf("----- FILE:  %s ------\n", _filename.c_str());
  }

  if (!init()) {
    printf("Log init() failed.");
    goto theend;
  }

  clearerr(_filePtr);
  rewind(_filePtr);
  if (ferror(_filePtr)) {
    printf("Error rewinding log: %s: %s\n", _filename.c_str(), strerror(errno));
    goto theend;
  }

  while (fgets(str, MAXCHAR, _filePtr) != NULL) {
    printf("%s", str);
  }
  if (ferror(_filePtr)) {
    printf("Error reading log: %s: %s\n", _filename.c_str(), strerror(errno));
    goto theend;
  }

theend:
  if (header) {
    printf("----- END LOG ------\n");
  }
}

String LogFileClass::tail(int lines) {

  int count = 0; // To count '\n' characters

  if (!init()) {
    return "";
  }
  const int MAXLINE = 256;
  unsigned long long pos;
  char str[MAXLINE];
  String r;

  // Go to End of file
  if (fseek(_filePtr, 0, SEEK_END)) {
    perror("fseek() failed");
  } else {
    // pos will contain no. of chars in    // input file.
    pos = ftell(_filePtr);
    // search for '\n' characters
    while (pos) {
      // Move 'pos' away from end of file.
      if (!fseek(_filePtr, --pos, SEEK_SET)) {
        if (fgetc(_filePtr) == '\n') {
          // stop reading when n newlinesis found
          if (count++ == lines)
            break;
        }
      } else {
        perror("fseek() failed");
      }
    }
    while (fgets(str, sizeof(str), _filePtr)) {
      r += str;
    }
  }
  r += "\n";
  return r;
}

/**
 * @todo
 * too slow... reads one char at a time, slow on spiffs
 * maybe reading backwards is slow.
 * consider enforcing line-length limit
*/
void LogFileClass::tail(FILE *out, int lines, bool header) {
  int count = 0; // To count '\n' characters

  const int MAXLINE = 256;
  unsigned long long pos;
  char str[MAXLINE];
  String r;

  if (!init()) {
    return;
  }
  // Go to End of file
  if (fseek(_filePtr, 0, SEEK_END)) {
    perror("fseek() failed");
  } else {
    // pos will contain no. of chars in    // input file.
    pos = ftell(_filePtr);
    // search for '\n' characters
    while (pos) {

      // Move 'pos' away from end of file.
      if (!fseek(_filePtr, --pos, SEEK_SET)) {
        if (fgetc(_filePtr) == '\n') {
          // stop reading when n newlines is found
          if (count++ == lines)
            break;
        }
      } else {
        perror("fseek() failed");
      }
    }
    if (header) {
      fprintf(out, "----- START LOG ------\n");
      fprintf(out, "----- FILE:  %s ------\n", _filename.c_str());
    }
    while (fgets(str, sizeof(str), _filePtr)) {
      str[strcspn(str, "\r\n")] = 0; // strip newlines (???)
      fprintf(out, "%s\n", str);
    }
  }
  if (header) {
    fprintf(out, "----- END LOG ------\n");
  }

  return;
}

String LogFileClass::head(int lines) {
  int count = 0; // To count '\n' characters

  if (!init()) {
    return "";
  }
  const int MAXLINE = 256;
  char str[MAXLINE];
  String r;

  while (fgets(str, sizeof(str), _filePtr) && (count++ < lines)) {
    r += str;
  }
  r += "\n";
  return r;
}

String resetReason(int core) {
  assert(core == 1 || core == 0);
  switch (rtc_get_reset_reason(core)) {
  case 1:
    return String("POWERON_RESET"); /**<1, Vbat power on reset*/
  case 3:
    return String("SW_RESET"); /**<3, Software reset digital core*/
  case 4:
    return String("OWDT_RESET"); /**<4, Legacy watch dog reset digital core*/
  case 5:
    return String("DEEPSLEEP_RESET"); /**<5, Deep Sleep reset digital core*/
  case 6:
    return String(
        "SDIO_RESET"); /**<6, Reset by SLC module, reset digital core*/
  case 7:
    return String(
        "TG0WDT_SYS_RESET"); /**<7, Timer Group0 Watch dog reset digital core*/
  case 8:
    return String("TG1WDT_SYS_RESET");
    ; /**<8, Timer Group1 Watch dog reset digital core*/
  case 9:
    return String("RTCWDT_SYS_RESET"); /**<9, RTC Watch dog Reset digital core*/
  case 10:
    return String("INTRUSION_RESET"); /**<10, Instrusion tested to reset CPU*/
  case 11:
    return String("TGWDT_CPU_RESET"); /**<11, Time Group reset CPU*/
  case 12:
    return String("SW_CPU_RESET"); /**<12, Software reset CPU*/
  case 13:
    return String("RTCWDT_CPU_RESET"); /**<13, RTC Watch dog Reset CPU*/
  case 14:
    return String("EXT_CPU_RESET"); /**<14, for APP CPU, reseted by PRO CPU*/
  case 15:
    return String("RTCWDT_BROWN_OUT_RESET"); /**<15, Reset when the vdd
                                                voltage is not stable*/
  case 16:
    return String("RTCWDT_RTC_RESET"); /**<16, RTC Watch dog reset digital
                                          core and rtc module*/
  default:
    return String("NO_MEAN");
  }
}

// This example demonstrates how a human readable table of run time stats
// information is generated from raw data provided by uxTaskGetSystemState().
// The human readable table is written to pcWriteBuffer
void vTaskPrintRunTimeStats() {

  TaskStatus_t *pxTaskStatusArray;
  volatile UBaseType_t uxArraySize, x;
  uint32_t ulTotalRunTime, ulStatsAsPercentage;
  eTaskState state;
  UBaseType_t currentPriority;

  // Take a snapshot of the number of tasks in case it changes while this
  // function is executing.
  uxArraySize = uxTaskGetNumberOfTasks();

  // Allocate a TaskStatus_t structure for each task.  An array could be
  // allocated statically at compile time.
  pxTaskStatusArray =
      (TaskStatus_t *)pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

  if (pxTaskStatusArray != NULL) {
    // Generate raw status information about each task.
    uxArraySize =
        uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

    // For percentage calculations.
    ulTotalRunTime /= 100UL;

    // Avoid divide by zero errors.
    if (ulTotalRunTime > 0) {
      // For each populated position in the pxTaskStatusArray array,
      // format the raw data as human readable ASCII data
      for (x = 0; x < uxArraySize; x++) {
        // What percentage of the total run time has the task used?
        // This will always be rounded down to the nearest integer.
        // ulTotalRunTimeDiv100 has already been divided by 100.
        ulStatsAsPercentage =
            pxTaskStatusArray[x].ulRunTimeCounter / ulTotalRunTime;

        if (ulStatsAsPercentage > 0UL) {
          printf("%-20s     %9u%10u%% %7d", pxTaskStatusArray[x].pcTaskName,
                 pxTaskStatusArray[x].ulRunTimeCounter, ulStatsAsPercentage,
                 (int)pxTaskStatusArray[x].usStackHighWaterMark);
        } else {
          // If the percentage is zero here then the task has
          // consumed less than 1% of the total run time.
          printf("%-20s     %10u        <1%% %7d",
                 pxTaskStatusArray[x].pcTaskName,
                 pxTaskStatusArray[x].ulRunTimeCounter,
                 (int)pxTaskStatusArray[x].usStackHighWaterMark);
        }
        state = pxTaskStatusArray[x].eCurrentState;
        currentPriority = pxTaskStatusArray[x].uxCurrentPriority;
        printf("s:%d p:%d(%d)\n", state, currentPriority, currentPriority);
      }
    }

    // The array is no longer needed, free the memory it consumes.
    vPortFree(pxTaskStatusArray);
  }
}

String getFilename(String path, String ps) {

  int indexOfLastPS;
  String deslashed;

  if (path == ps || path == "")
    return ("");

  deslashed = slash("", path, ""); // remove last slash,
  indexOfLastPS = deslashed.lastIndexOf(ps);
  // Serial.println(path);Serial.println(deslashed);Serial.println(indexOfLastPS);
  if (indexOfLastPS == -1) {
    return path;
  } else {
    return deslashed.substring(indexOfLastPS + 1);
  }
}

bool timeIsValid() {
  time_t now;
  struct tm timeinfo;
  time(&now);
  localtime_r(&now, &timeinfo);
  // tm_year is years since 1900
  return (timeinfo.tm_year > (2018 - 1900));
}

String timestamp() {
  time_t now;
  struct tm timeinfo;
  char strftime_buf[64];
  time(&now);
  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H-%M-%S", &timeinfo);
  return String(strftime_buf);
}

String timestamp(time_t now) {

  struct tm timeinfo;
  char strftime_buf[64];

  localtime_r(&now, &timeinfo);
  strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H-%M-%S", &timeinfo);
  return String(strftime_buf);
}

bool waitForNTP(int retries) {
  int retry = 0;

  vTaskDelay(1000 / portTICK_PERIOD_MS);
  while (!timeIsValid() && retries < retries) {
    ESP_LOGI(TAG, "Waiting for NTP time to be set... (%d/%d)", retry, retries);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
  return timeIsValid();
}

} // namespace bbesp32lib
