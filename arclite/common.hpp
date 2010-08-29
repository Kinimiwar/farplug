#pragma once

struct Attr {
  wstring name;
  wstring value;
};
typedef list<Attr> AttrList;

class ErrorLog: public map<wstring, Error> {
public:
  void add(const wstring& file_path, const Error& e) {
    if (count(file_path) == 0)
      (*this)[file_path] = e;
  }
};

class ProgressMonitor;
bool retry_or_ignore_error(const wstring& path, const Error& error, bool& ignore_errors, ErrorLog& error_log, ProgressMonitor& progress);
void ignore_error(const wstring& path, const Error& error, bool& ignore_errors, ErrorLog& error_log, ProgressMonitor& progress);

unsigned calc_percent(unsigned __int64 completed, unsigned __int64 total);
unsigned __int64 get_module_version(const wstring& file_path);
