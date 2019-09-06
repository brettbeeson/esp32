#pragma once

#include <Arduino.h>

#include <vector>

namespace bbesp32lib {
void vTaskPrintRunTimeStats();
void taskState(TaskHandle_t task);
const String ps = "/";
String slash(const String &stripFromStart, const String &from,
             const String &stripFromEnd);
String slash(char withStart, const String &from);
String slash(const String &from, char withEnd);
std::vector<String> ls(const String &localDir, int maxFiles,
                       const String &endingWith = "");
bool lsStats(const String &localDir, int *nFiles, String *firstFile,
             String *lastFile, const String &endingWith = "");
unsigned long mround_ul(unsigned long n, int m);
time_t mround_time_t(time_t t, int s);
// Return microseconds (1e6) to a future time which is the next multiple of
// secondsMultiple
uint64_t uSecToNextMultiple(time_t secondsMultiple);
String resetReason(int core = 0);
String getFilename(String path, String ps = "/");

String timestamp();
String timestamp(time_t t);
bool timeIsValid();
bool waitForNTP(int retries = 10);

class LogFileClass {
public:
  LogFileClass();
  ~LogFileClass();
  LogFileClass(const char *filename);
  // void printf(const char* message, ...);
  void captureESPLog(bool alsoWriteToStdout);
  void print(const String &message);
  int vprintf(const char *fmt, va_list args);
  // void print(const char* message);
  void read(bool header=false);
  String tail(int lines = 10);
  String head(int lines = 10);
  void tail(FILE *out, int lines = 10, bool header = false);
  //vprintf_like_t
  //typedef int (*vprintf_like_t)(const char *, va_list);
  // Use with esp_log_set_vprintf()
  static int vprintfHook(const char *, va_list);
  bool vprintfCopyToStdout = false;
  void setFilename(const char *filename);
  String getFilename();

private:
  String _filename;
  FILE *_filePtr = NULL;
  bool init();
  int lastError=0;
};

extern LogFileClass LogFile;

class BlinkClass {
public:
  enum Speed {
    SLOTH = 1,
    HUMAN = 5,
    HARE = 25
  };
  enum Duration {
    SHORT = 10,
    MEDIUM = 100,
    LONG = 1000
  };

  BlinkClass();
  void begin(gpio_num_t pin = GPIO_NUM_2);
  void end();
  void set(float freqHz, float onMs = 100, bool heartbeat = false);
  void flash(Duration d = MEDIUM);
  void flash(int n, Speed s = HUMAN, Duration d = MEDIUM);
  friend void BlinkTask(void *blinkClassPtr);

private:
  static const int ON = 1;
  static const int OFF = 0;
  int _onMs = 0;
  int _offMs = 0;
  bool _on = false;
  gpio_num_t _pin = GPIO_NUM_0;
};

extern BlinkClass Blink;

} // namespace bbesp32lib
