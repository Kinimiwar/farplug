#include "msg.h"
#include "utils.hpp"
#include "farutils.hpp"
#include "sysutils.hpp"
#include "common.hpp"
#include "archive.hpp"
#include "ui.hpp"

class AttachSfxModuleProgress: public ProgressMonitor {
private:
  wstring file_path;
  unsigned __int64 completed;
  unsigned __int64 total;

  virtual void do_update_ui() {
    const unsigned c_width = 60;
    wostringstream st;
    st << Far::get_msg(MSG_PLUGIN_NAME) << L'\n';

    unsigned percent = calc_percent(completed, total);

    st << Far::get_msg(MSG_PROGRESS_SFX_CONVERT) << L'\n';
    st << fit_str(file_path, c_width) << L'\n';
    st << Far::get_progress_bar_str(c_width, percent, 100) << L'\n';

    Far::message(st.str(), 0, FMSG_LEFTALIGN);

    Far::set_progress_state(TBPF_NORMAL);
    Far::set_progress_value(percent, 100);

    SetConsoleTitleW((L"{" + int_to_str(percent) + L"%} " + Far::get_msg(MSG_PROGRESS_SFX_CONVERT)).c_str());
  }

public:
  AttachSfxModuleProgress(const wstring& file_path): ProgressMonitor(true), file_path(file_path), completed(0), total(0) {
  }

  void set_total(unsigned __int64 size) {
    total = size;
  }
  void update_completed(unsigned __int64 size) {
    completed += size;
    update_ui();
  }
};

void append_file(const wstring& src_path, File& dst_file, AttachSfxModuleProgress& progress) {
  File source(src_path, FILE_READ_DATA, FILE_SHARE_READ, OPEN_EXISTING, 0);
  Buffer<char> buf(1024 * 1024);
  while (true) {
    unsigned size_read = source.read(buf.data(), static_cast<unsigned>(buf.size()));
    if (size_read == 0)
      break;
    CHECK(dst_file.write(buf.data(), size_read) == size_read);
    progress.update_completed(size_read);
  }
}

void attach_sfx_module(const wstring& file_path, unsigned sfx_module_idx) {
  AttachSfxModuleProgress progress(file_path);

  {
    vector<Archive> archives = Archive::detect(file_path, false);
    if (archives.empty())
      FAIL_MSG(Far::get_msg(MSG_ERROR_SFX_CONVERT));
    if (!archives.front().is_pure_7z())
      FAIL_MSG(Far::get_msg(MSG_ERROR_SFX_CONVERT));
  }

  const SfxModules& sfx_modules = ArcAPI::sfx();
  CHECK(sfx_module_idx < sfx_modules.size());
  wstring sfx_module_path = sfx_modules[sfx_module_idx].path;

  FindData file_data = get_find_data(file_path);
  progress.set_total(file_data.size());

  wstring dst_path = file_path + c_sfx_ext;
  try {
    File dst_file(dst_path, FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, CREATE_NEW, FILE_ATTRIBUTE_NORMAL);
    append_file(sfx_module_path, dst_file, progress);
    append_file(file_path, dst_file, progress);
    CHECK_SYS(SetFileAttributesW(long_path(file_path).c_str(), file_data.dwFileAttributes));
    dst_file.set_time(file_data.ftCreationTime, file_data.ftLastAccessTime, file_data.ftLastWriteTime);
  }
  catch (...) {
    DeleteFileW(long_path(dst_path).c_str());
    throw;
  }
  DeleteFileW(long_path(file_path).c_str());
}

class SfxConvertDialog: public Far::Dialog {
private:
  enum {
    c_client_xs = 69
  };

  unsigned& sfx_module_idx;

  int sfx_module_ctrl_id;
  int ok_ctrl_id;
  int cancel_ctrl_id;

  LONG_PTR dialog_proc(int msg, int param1, LONG_PTR param2) {
    if (msg == DN_CLOSE && param1 >= 0 && param1 != cancel_ctrl_id) {
      sfx_module_idx = get_list_pos(sfx_module_ctrl_id);
      if (sfx_module_idx >= ArcAPI::sfx().size()) {
        FAIL_MSG(Far::get_msg(MSG_UPDATE_DLG_WRONG_SFX_MODULE));
      }
    }
    return default_dialog_proc(msg, param1, param2);
  }

public:
  SfxConvertDialog(unsigned& sfx_module_idx): Far::Dialog(Far::get_msg(MSG_SFX_CONVERT_DLG_TITLE), c_client_xs), sfx_module_idx(sfx_module_idx) {
  }

  bool show() {
    label(Far::get_msg(MSG_SFX_CONVERT_DLG_MODULE));
    new_line();
    vector<wstring> sfx_module_list;
    const SfxModules& sfx_modules = ArcAPI::sfx();
    for_each(sfx_modules.begin(), sfx_modules.end(), [&] (const SfxModule& sfx_module) {
      sfx_module_list.push_back(sfx_module.path);
    });
    sfx_module_ctrl_id = combo_box(sfx_module_list, sfx_module_idx, c_client_xs, DIF_DROPDOWNLIST);
    new_line();

    separator();
    new_line();
    ok_ctrl_id = def_button(Far::get_msg(MSG_BUTTON_OK), DIF_CENTERGROUP);
    cancel_ctrl_id = button(Far::get_msg(MSG_BUTTON_CANCEL), DIF_CENTERGROUP);
    new_line();

    int item = Far::Dialog::show();

    return (item != -1) && (item != cancel_ctrl_id);
  }
};

bool sfx_convert_dialog(unsigned& sfx_module_idx) {
  return SfxConvertDialog(sfx_module_idx).show();
}