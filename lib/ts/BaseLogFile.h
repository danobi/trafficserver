/** @file

  Base class for log files

  @section license License

  Licensed to the Apache Software Foundation (ASF) under one
  or more contributor license agreements.  See the NOTICE file
  distributed with this work for additional information
  regarding copyright ownership.  The ASF licenses this file
  to you under the Apache License, Version 2.0 (the
  "License"); you may not use this file except in compliance
  with the License.  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

 */


#ifndef BASE_LOG_FILE_H
#define BASE_LOG_FILE_H

#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "ink_memory.h"
#include "ink_string.h"
#include "ink_file.h"
#include "ink_cap.h"
#include "ink_time.h"
#include "SimpleTokenizer.h"

#define LOGFILE_ROLLED_EXTENSION ".old"
#define LOGFILE_SEPARATOR_STRING "_"
#define LOGFILE_DEFAULT_PERMS (0644)
#define LOGFILE_ROLL_MAXPATHLEN 4096

typedef enum {
  LL_Debug = 0, // process does not die
  LL_Note,      // process does not die
  LL_Warning,   // process does not die
  LL_Error,     // process does not die
  LL_Fatal,     // causes process termination
} LogLogPriorityLevel;

#define log_log_trace(...)                         \
  do {                                             \
    if (1)                                         \
      BaseLogFile::log_log(LL_Debug, __VA_ARGS__); \
  } while (0)

#define log_log_error(...)                         \
  do {                                             \
    if (1)                                         \
      BaseLogFile::log_log(LL_Error, __VA_ARGS__); \
  } while (0)

/*
 *
 * BaseMetaInfo class
 *
 * Used to store persistent information between ATS instances
 *
 */
class BaseMetaInfo
{
public:
  enum {
    DATA_FROM_METAFILE = 1, // metadata was read (or attempted to)
    // from metafile
    VALID_CREATION_TIME = 2, // creation time is valid
    VALID_SIGNATURE = 4,     // signature is valid
    // (i.e., creation time only)
    FILE_OPEN_SUCCESSFUL = 8 // metafile was opened successfully
  };

  enum {
    BUF_SIZE = 640 // size of read/write buffer
  };

private:
  char *_filename;                // the name of the meta file
  time_t _creation_time;          // file creation time
  uint64_t _log_object_signature; // log object signature
  int _flags;                     // metainfo status flags
  char _buffer[BUF_SIZE];         // read/write buffer

  void _read_from_file();
  void _write_to_file();
  void _build_name(const char *filename);

public:
  BaseMetaInfo(const char *filename) : _flags(0)
  {
    _build_name(filename);
    _read_from_file();
  }

  BaseMetaInfo(char *filename, time_t creation) : _creation_time(creation), _flags(VALID_CREATION_TIME)
  {
    _build_name(filename);
    _write_to_file();
  }

  BaseMetaInfo(char *filename, time_t creation, uint64_t signature)
    : _creation_time(creation), _log_object_signature(signature), _flags(VALID_CREATION_TIME | VALID_SIGNATURE)
  {
    _build_name(filename);
    _write_to_file();
  }

  ~BaseMetaInfo() { ats_free(_filename); }

  bool
  get_creation_time(time_t *time)
  {
    if (_flags & VALID_CREATION_TIME) {
      *time = _creation_time;
      return true;
    } else {
      return false;
    }
  }

  bool
  get_log_object_signature(uint64_t *signature)
  {
    if (_flags & VALID_SIGNATURE) {
      *signature = _log_object_signature;
      return true;
    } else {
      return false;
    }
  }

  bool
  data_from_metafile() const
  {
    return (_flags & DATA_FROM_METAFILE ? true : false);
  }

  bool
  file_open_successful()
  {
    return (_flags & FILE_OPEN_SUCCESSFUL ? true : false);
  }
};


/*
 *
 * BaseLogFile Class
 *
 */
class BaseLogFile
{
public:
  // member functions
  BaseLogFile(const char *name, bool is_bootstrap);
  BaseLogFile(const BaseLogFile &);
  ~BaseLogFile();
  int roll(long interval_start, long interval_end);
  static bool rolled_logfile(char *path);
  static bool exists(const char *pathname);
  int open_file();
  void close_file();
  void change_name(const char *new_name);
  void display(FILE *fd = stdout);
  const char *
  get_name() const
  {
    return m_name;
  }
  bool
  is_open()
  {
    return (m_fd >= 0);
  }
  off_t
  get_size_bytes() const
  {
    // XXX FIXME
    // return m_file_format != LOG_FILE_PIPE ? m_bytes_written : 0;
    return 0;
  };
  static void log_log(LogLogPriorityLevel priority, const char *format, ...);

  // member variables
  enum {
    LOG_FILE_NO_ERROR = 0,
    LOG_FILE_COULD_NOT_OPEN_FILE,
  };

  int m_fd;
  long m_start_time;
  long m_end_time;
  volatile uint64_t m_bytes_written;

private:
  // member functions not allowed
  BaseLogFile();
  BaseLogFile &operator=(const BaseLogFile &);

  // member functions
  // XXX temp function: move to a better place
  /*-------------------------------------------------------------------------
    LogUtils::timestamp_to_str

    This routine will convert a timestamp (seconds) into a short string,
    of the format "%Y%m%d.%Hh%Mm%Ss".

    Since the resulting buffer is passed in, this routine is thread-safe.
    Return value is the number of characters placed into the array, not
    including the NULL.
    -------------------------------------------------------------------------*/

  int timestamp_to_str_2(long timestamp, char *buf, int size)
  {
    static const char *format_str = "%Y%m%d.%Hh%Mm%Ss";
    struct tm res;
    struct tm *tms;
    tms = ink_localtime_r((const time_t *)&timestamp, &res);
    return strftime(buf, size, format_str, tms);
  }

  // member variables
  char *m_name;
  uint64_t m_signature;
  bool m_is_bootstrap;
  BaseMetaInfo *m_meta_info;
};
#endif
