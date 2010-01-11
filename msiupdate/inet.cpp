#include "msg.h"

#include "utils.hpp"
#include "sysutils.hpp"
#include "farutils.hpp"
#include "options.hpp"
#include "ui.hpp"
#include "inet.hpp"

class HInternet: private NonCopyable {
private:
  HINTERNET handle;
  void close() {
    if (handle) {
      WinHttpSetStatusCallback(handle, NULL, 0, 0);
      WinHttpCloseHandle(handle);
      handle = NULL;
    }
  }
public:
  HInternet(): handle(NULL) {
  }
  HInternet(HINTERNET h): handle(h) {
  }
  ~HInternet() {
    close();
  }
  HInternet& operator=(HINTERNET h) {
    close();
    handle = h;
    return *this;
  }
  operator HINTERNET() const {
    return handle;
  }
  operator bool() const {
    return handle != NULL;
  }
};

struct Context {
  HANDLE h_complete;
  string data;
  DWORD error;
  bool more_data;
  Context() {
    error = ERROR_SUCCESS;
    more_data = true;
    h_complete = CreateEvent(NULL, FALSE, FALSE, NULL);
    CHECK_SYS(h_complete != NULL);
  }
  ~Context() {
    CloseHandle(h_complete);
  }
};

void CALLBACK status_callback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus, LPVOID lpvStatusInformation, DWORD dwStatusInformationLength) {
  Context* context = reinterpret_cast<Context*>(dwContext);
  if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_READ_COMPLETE) {
    if (dwStatusInformationLength)
      context->data.append(reinterpret_cast<char*>(lpvStatusInformation), dwStatusInformationLength);
    else
      context->more_data = false;
  }
  else if (dwInternetStatus == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR) {
    context->error = reinterpret_cast<WINHTTP_ASYNC_RESULT*>(lpvStatusInformation)->dwError;
  }
  SetEvent(context->h_complete);
}

void wait(Context& context, HANDLE h_abort) {
  HANDLE h_wait[] = { h_abort, context.h_complete };
  DWORD wait_res = WaitForMultipleObjects(ARRAYSIZE(h_wait), h_wait, FALSE, INFINITE);
  CHECK_SYS(wait_res != WAIT_FAILED);
  if (wait_res == WAIT_OBJECT_0) {
    FAIL(E_ABORT);
  }
  else if (wait_res == WAIT_OBJECT_0 + 1) {
    CHECK_ADVSYS(context.error);
  }
  else {
    FAIL(E_FAIL);
  }
}

const wchar_t* c_user_agent = L"MsiUpdate";

string load_url(const wstring& url, const HttpOptions& options, HANDLE h_abort, LoadUrlProgress* progress) {
  DWORD proxy_type;
  LPCWSTR proxy_name;
  wstring address;
  if (options.use_proxy && !options.proxy_server.empty()) {
    address = options.proxy_server;
    if (options.proxy_port)
      address.append(L":").append(int_to_str(options.proxy_port));
    proxy_type = WINHTTP_ACCESS_TYPE_NAMED_PROXY;
    proxy_name = address.c_str();
  }
  else {
    proxy_type = WINHTTP_ACCESS_TYPE_DEFAULT_PROXY;
    proxy_name = WINHTTP_NO_PROXY_NAME;
  }
  HInternet h_session = WinHttpOpen(c_user_agent, proxy_type, proxy_name, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
  CHECK_SYS(h_session);

  Context context;
  CHECK_SYS(WinHttpSetStatusCallback(h_session, status_callback, WINHTTP_CALLBACK_FLAG_ALL_COMPLETIONS, 0) != WINHTTP_INVALID_STATUS_CALLBACK);

  URL_COMPONENTS url_parts;
  memset(&url_parts, 0, sizeof(url_parts));
  url_parts.dwStructSize = sizeof(url_parts);
  url_parts.dwHostNameLength = 1;
  url_parts.dwUrlPathLength = 1;
  CHECK_SYS(WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.size()), 0, &url_parts));

  HInternet h_conn = WinHttpConnect(h_session, wstring(url_parts.lpszHostName, url_parts.dwHostNameLength).c_str(), url_parts.nPort, 0);
  CHECK_SYS(h_conn);

  HInternet h_request = WinHttpOpenRequest(h_conn, NULL, wstring(url_parts.lpszUrlPath, url_parts.dwUrlPathLength).c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_BYPASS_PROXY_CACHE | WINHTTP_FLAG_REFRESH);
  CHECK_SYS(h_request);

  if (options.use_proxy && !options.proxy_user_name.empty()) {
    DWORD auth_scheme;
    switch (options.proxy_auth_scheme) {
    case 0: auth_scheme = WINHTTP_AUTH_SCHEME_BASIC; break;
    case 1: auth_scheme = WINHTTP_AUTH_SCHEME_NTLM; break;
    case 2: auth_scheme = WINHTTP_AUTH_SCHEME_PASSPORT; break;
    case 3: auth_scheme = WINHTTP_AUTH_SCHEME_DIGEST; break;
    case 4: auth_scheme = WINHTTP_AUTH_SCHEME_NEGOTIATE; break;
    default: auth_scheme = WINHTTP_AUTH_SCHEME_BASIC; break;
    }
    CHECK_SYS(WinHttpSetCredentials(h_request, WINHTTP_AUTH_TARGET_PROXY, auth_scheme, options.proxy_user_name.c_str(), options.proxy_password.c_str(), NULL));
  }

  CHECK_SYS(WinHttpSendRequest(h_request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, reinterpret_cast<DWORD_PTR>(&context)));
  wait(context, h_abort);

  CHECK_SYS(WinHttpReceiveResponse(h_request, NULL));
  wait(context, h_abort);

  DWORD status;
  DWORD status_size = sizeof(status);
  CHECK_SYS(WinHttpQueryHeaders(h_request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX));
  if (status != HTTP_STATUS_OK) {
    wostringstream st;
    st << L"Server returned error code: " << status;

    Buffer<char> status_text(100);
    DWORD status_text_size = static_cast<DWORD>(status_text.size());
    BOOL res = WinHttpQueryHeaders(h_request, WINHTTP_QUERY_STATUS_TEXT, WINHTTP_HEADER_NAME_BY_INDEX, status_text.data(), &status_text_size, WINHTTP_NO_HEADER_INDEX);
    if (!res && (GetLastError() == ERROR_INSUFFICIENT_BUFFER)) {
      status_text.resize(status_text_size);
      res = WinHttpQueryHeaders(h_request, WINHTTP_QUERY_STATUS_TEXT, WINHTTP_HEADER_NAME_BY_INDEX, status_text.data(), &status_text_size, WINHTTP_NO_HEADER_INDEX);
    }
    if (res)
      st << L" " << wstring(reinterpret_cast<wchar_t*>(status_text.data()), status_text_size / sizeof(wchar_t));
    FAIL_MSG(st.str());
  }

  const unsigned c_max_clen = 100 * 1024 * 1024;
  DWORD clen;
  DWORD clen_size = sizeof(clen);
  if (WinHttpQueryHeaders(h_request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &clen, &clen_size, WINHTTP_NO_HEADER_INDEX)) {
    if (clen < c_max_clen)
      context.data.reserve(clen);
    progress->set(0, clen);
  }

  const unsigned c_read_buf_size = 64 * 1024;
  Buffer<char> buf(c_read_buf_size);
  do {
    CHECK_SYS(WinHttpReadData(h_request, buf.data(), static_cast<DWORD>(buf.size()), NULL));
    wait(context, h_abort);
    progress->set(static_cast<unsigned>(context.data.size()), clen);
  }
  while (context.more_data);

  return context.data;
}

struct LoadUrlContext {
  wstring url;
  HttpOptions options;
  HANDLE h_abort;
  LoadUrlProgress* progress;
  string result;
  Error error;
};

unsigned __stdcall load_url_thread(void* param) {
  LoadUrlContext* ctx = reinterpret_cast<LoadUrlContext*>(param);
  try {
    try {
      ctx->result = load_url(ctx->url, ctx->options, ctx->h_abort, ctx->progress);
      return TRUE;
    }
    catch (const Error&) {
      throw;
    }
    catch (const std::exception& e) {
      FAIL_MSG(widen(e.what()));
    }
    catch (...) {
      FAIL(E_FAIL);
    }
  }
  catch (const Error& e) {
    ctx->error = e;
  }
  catch (...) {
  }
  return FALSE;
}

string load_url(const wstring& url, const HttpOptions& options) {
  HANDLE h_abort = CreateEvent(NULL, TRUE, FALSE, NULL);
  CHECK_SYS(h_abort);
  CleanHandle h_abort_clean(h_abort);
  LoadUrlProgress progress;
  LoadUrlContext ctx;
  ctx.url = url;
  ctx.options = options;
  ctx.h_abort = h_abort;
  ctx.progress = &progress;
  unsigned th_id;
  HANDLE h_thread = reinterpret_cast<HANDLE>(_beginthreadex(NULL, 0, load_url_thread, &ctx, 0, &th_id));
  CHECK_SYS(h_thread);
  CleanHandle h_thread_clean(h_thread);
  try {
    while (true) {
      const unsigned c_wait_time = 100;
      if (wait_for_single_object(h_thread, c_wait_time))
        break;
      progress.update_ui();
    }
  }
  catch (const Error& e) {
    if (e.code == E_ABORT) {
      CHECK_SYS(SetEvent(h_abort));
      wait_for_single_object(h_thread, INFINITE);
    }
    throw;
  }
  DWORD exit_code;
  CHECK_SYS(GetExitCodeThread(h_thread, &exit_code));
  if (exit_code)
    return ctx.result;
  else
    throw ctx.error;
}

void LoadUrlProgress::do_update_ui() {
  unsigned completed, total;
  {
    CriticalSectionLock cs_lock(*this);
    completed = this->completed;
    total = this->total;
  }
  wostringstream msg;
  msg << Far::get_msg(MSG_PLUGIN_NAME) << L'\n';
  msg << Far::get_msg(MSG_DOWNLOAD_MESSAGE);
  if (completed || total) {
    msg << L": " << completed;
    if (total)
      msg << L"/" << total;
  }
  else
    msg << L"...";
  msg << L'\n';
  if (total)
    msg << Far::get_progress_bar_str(60, completed, total);
  Far::message(msg.str(), 0, FMSG_LEFTALIGN);
  msg.str(wstring());
  msg << Far::get_msg(MSG_DOWNLOAD_TITLE);
  if (total && completed <= total) {
    msg << L": " << completed * 100 / total << L"%";
    Far::set_progress_state(TBPF_NORMAL);
    Far::set_progress_value(completed, total);
  }
  else {
    msg << L"...";
    Far::set_progress_state(TBPF_INDETERMINATE);
  }
  SetConsoleTitleW(msg.str().c_str());
}
