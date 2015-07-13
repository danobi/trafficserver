/** @file

  Base class for log files implementation

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

#include "BaseLogFile.h"

/*
 * This consturctor creates a BaseLogFile based on a given name.
 * This is the most common way BaseLogFiles are created.
 */
BaseLogFile::BaseLogFile(const char *name, bool is_bootstrap) : m_name(ats_strdup(name)), m_is_bootstrap(is_bootstrap)
{
  m_fd = -1;
  m_start_time = 0L;
  m_end_time = 0L;
  m_bytes_written = 0;
  m_meta_info = NULL;

  log_log_trace("exiting BaseLogFile constructor, m_name=%s, this=%p\n", m_name, this);
}

/*
 * This copy constructor creates a BaseLogFile based on a given copy.
 */
BaseLogFile::BaseLogFile(const BaseLogFile &copy)
  : m_fd(-1), m_start_time(0L), m_end_time(0L), m_bytes_written(0), m_name(ats_strdup(copy.m_name)),
    m_is_bootstrap(copy.m_is_bootstrap), m_meta_info(NULL)
{
  log_log_trace("exiting BaseLogFile copy constructor, m_name=%s, this=%p\n", m_name, this);
}

/*
 * Destructor.
 */
BaseLogFile::~BaseLogFile()
{
  log_log_trace("entering BaseLogFile destructor, this=%p\n", this);

  close_file();
  ats_free(m_name);

  log_log_trace("exiting BaseLogFile destructor, this=%p\n", this);
}

/*
 * This function is called by a client of BaseLogFile to roll the underlying
 * file  The tricky part to this routine is in coming up with the new file name,
 * which contains the bounding timestamp interval for the entries
 * within the file.

 * Under normal operating conditions, this BaseLogFile object was in existence
 * for all writes to the file.  In this case, the LogFile members m_start_time
 * and m_end_time will have the starting and ending times for the actual
 * entries written to the file.

 * On restart situations, it is possible to re-open an existing BaseLogFile,
 * which means that the m_start_time variable will be later than the actual
 * entries recorded in the file.  In this case, we'll use the creation time
 * of the file, which should be recorded in the meta-information located on
 * disk.

 * If we can't use the meta-file, either because it's not there or because
 * it's not valid, then we'll use timestamp 0 (Jan 1, 1970) as the starting
 * bound.

 * Return 1 if file rolled, 0 otherwise
 */
int
BaseLogFile::roll(long interval_start, long interval_end)
{
  return 0;
}

/*
 * This function will return true if the given filename corresponds to a
 * rolled logfile.  We make this determination based on the file extension.
 */
bool
BaseLogFile::rolled_logfile(char *path)
{
  const int target_len = (int)strlen(LOGFILE_ROLLED_EXTENSION);
  int len = (int)strlen(path);
  if (len > target_len) {
    char *str = &path[len - target_len];
    if (!strcmp(str, LOGFILE_ROLLED_EXTENSION)) {
      return true;
    }
  }
  return false;
}

bool
BaseLogFile::exists(const char *pathname)
{
  return false;
}

int
BaseLogFile::open_file()
{
  if (is_open()) {
    return LOG_FILE_NO_ERROR;
  }

  if (m_name && !strcmp(m_name, "stdout")) {
    m_fd = STDOUT_FILENO;
    return LOG_FILE_NO_ERROR;
  }
  //
  // Check to see if the file exists BEFORE we try to open it, since
  // opening it will also create it.
  //
  bool file_exists = BaseLogFile::exists(m_name);

  if (file_exists) {
    if (!m_meta_info) {
      // This object must be fresh since it has not built its MetaInfo
      // so we create a new MetaInfo object that will read right away
      // (in the constructor) the corresponding metafile
      //
      m_meta_info = new BaseMetaInfo(m_name);
    }
  } else {
    // The log file does not exist, so we create a new MetaInfo object
    //  which will save itself to disk right away (in the constructor)
    m_meta_info = new BaseMetaInfo(m_name, (long)time(0), m_signature);
  }

  int flags, perms;

  log_log_trace("attempting to open %s\n", m_name);
  m_fd = ::open(m_name, flags, perms);

  if (m_fd < 0) {
    log_log_error("Error opening log file %s: %s\n", m_name, strerror(errno));
    return LOG_FILE_COULD_NOT_OPEN_FILE;
  }

  // set m_bytes_written to force the rolling based on filesize.
  m_bytes_written = lseek(m_fd, 0, SEEK_CUR);

  log_log_trace("BaseLogFile %s is now open (fd=%d)\n", m_name, m_fd);
  return LOG_FILE_NO_ERROR;
}

void
BaseLogFile::close_file()
{
  return;
}

void
BaseLogFile::check_fd()
{
  return;
}

void
BaseLogFile::change_name(const char *new_name)
{
  return;
}

void
BaseLogFile::display(FILE *fd)
{
  return;
}

/*
 * Lowest level internal logging facility for BaseLogFile
 *
 * Since BaseLogFiles can potentially be created before the bootstrap
 * instance of Diags is ready, we cannot simply call something like Debug().
 * However, we still need to log the creation of BaseLogFile, since the
 * information is still useful. This function will print out log messages
 * into traffic.out if we happen to be bootstrapping Diags. Since
 * traffic_cop redirects stdout/stderr into traffic.out, that
 * redirection is inherited by way of exec()/fork() all the way here.
 *
 * TODO use Debug() for non bootstrap instances
 */
void
BaseLogFile::log_log(LogLogPriorityLevel priority, const char *format, ...)
{
  va_list args;

  const char *priority_name = NULL;
  switch (priority) {
  case LL_Debug:
    priority_name = "DEBUG";
    break;
  case LL_Note:
    priority_name = "NOTE";
    break;
  case LL_Warning:
    priority_name = "WARNING";
    break;
  case LL_Error:
    priority_name = "ERROR";
    break;
  case LL_Fatal:
    priority_name = "FATAL";
    break;
  default:
    priority_name = "unknown priority";
  }

  va_start(args, format);
  struct timeval now;
  double now_f;

  gettimeofday(&now, NULL);
  now_f = now.tv_sec + now.tv_usec / 1000000.0f;

  fprintf(stdout, "<%.4f> [%s]: ", now_f, priority_name);
  vfprintf(stdout, format, args);
  fflush(stdout);

  va_end(args);
}



/****************************************************************************

  MetaInfo methods

*****************************************************************************/

/*
 * Given a file name (with or without the full path, but without the extension name),
 * this function creates appends the ".meta" extension and stores it in the local
 * variable
 */
void
BaseMetaInfo::_build_name(const char *filename)
{
  int i = -1, l = 0;
  char c;
  while (c = filename[l], c != 0) {
    if (c == '/') {
      i = l;
    }
    ++l;
  }

  // 7 = 1 (dot at beginning) + 5 (".meta") + 1 (null terminating)
  //
  _filename = (char *)ats_malloc(l + 7);

  if (i < 0) {
    ink_string_concatenate_strings(_filename, ".", filename, ".meta", NULL);
  } else {
    memcpy(_filename, filename, i + 1);
    ink_string_concatenate_strings(&_filename[i + 1], ".", &filename[i + 1], ".meta", NULL);
  }
}

/*
 * Reads in meta info into the local variables
 */
void
BaseMetaInfo::_read_from_file()
{
  _flags |= DATA_FROM_METAFILE;        // mark attempt
  int fd = open(_filename, O_RDONLY);
  if (fd < 0) {
    log_log_error("Could not open metafile %s for reading: %s\n", _filename, strerror(errno));
  } else {
    _flags |= FILE_OPEN_SUCCESSFUL;
    SimpleTokenizer tok('=', SimpleTokenizer::OVERWRITE_INPUT_STRING);
    int line_number = 1;
    while (ink_file_fd_readline(fd, BUF_SIZE, _buffer) > 0) {
      tok.setString(_buffer);
      char *t = tok.getNext();
      if (t) {
        if (strcmp(t, "creation_time") == 0) {
          t = tok.getNext();
          if (t) {
            _creation_time = (time_t)ink_atoi64(t);
            _flags |= VALID_CREATION_TIME;
          }
        } else if (strcmp(t, "object_signature") == 0) {
          t = tok.getNext();
          if (t) {
            _log_object_signature = ink_atoi64(t);
            _flags |= VALID_SIGNATURE;
            log_log_trace("BaseMetaInfo::_read_from_file\n"
                              "\tfilename = %s\n"
                              "\tsignature string = %s\n"
                              "\tsignature value = %" PRIu64 "\n",
                  _filename, t, _log_object_signature);
          }
        } else if (line_number == 1) {
          ink_release_assert(!"no panda support");
        }
      }
      ++line_number;
    }
    close(fd);
  }
}

/*
 * Writes out metadata info onto disk
 */
void
BaseMetaInfo::_write_to_file()
{
  // grab log file permissions
  int perms = 0644;
  // XXX implement permission resetting when core components come online
  /*
  if (!is_bootstrap) {
    char * ptr = REC_ConfigReadString("proxy.config.log.logfile_perm");
    int logfile_perm_parsed = ink_fileperm_parse(ptr);
    if (logfile_perm_parsed != -1)
      perms = logfile_perm_parsed;
    ats_free(ptr);
  }
  */

  int fd = open(_filename, O_WRONLY | O_CREAT | O_TRUNC, perms);
  if (fd < 0) {
    log_log_error("Could not open metafile %s for writing: %s\n", _filename, strerror(errno));
    return;
  }

  int n;
  if (_flags & VALID_CREATION_TIME) {
    n = snprintf(_buffer, BUF_SIZE, "creation_time = %lu\n", (unsigned long)_creation_time);
    // TODO modify this runtime check so that it is not an assertion
    ink_release_assert(n <= BUF_SIZE);
    if (write(fd, _buffer, n) == -1) {
      log_log_trace("Could not write creation_time");
    }
  }

  if (_flags & VALID_SIGNATURE) {
    n = snprintf(_buffer, BUF_SIZE, "object_signature = %" PRIu64 "\n", _log_object_signature);
    // TODO modify this runtime check so that it is not an assertion
    ink_release_assert(n <= BUF_SIZE);
    if (write(fd, _buffer, n) == -1) {
      log_log_error("Could not write object_signaure\n");
    }
    log_log_trace("BaseMetaInfo::_write_to_file\n"
                      "\tfilename = %s\n"
                      "\tsignature value = %" PRIu64 "\n"
                      "\tsignature string = %s\n",
          _filename, _log_object_signature, _buffer);
  }

  close(fd);
}
