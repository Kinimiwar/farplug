#include "msg.h"

#include "utils.hpp"
#include "sysutils.hpp"
#include "farutils.hpp"
#include "common_types.hpp"
#include "ui.hpp"
#include "archive.hpp"
#include "options.hpp"

class Plugin: private Archive {
private:
  wstring current_dir;
  wstring extract_dir;
  wstring host_file;
  wstring panel_title;

  bool open_file(const wstring& file_path, bool auto_detect) {
    max_check_size = g_options.max_check_size;
    vector<ArcFormatChain> format_chains = detect(file_path, !auto_detect);

    if (format_chains.size() == 0)
      return false;

    if (auto_detect) {
      format_chains.erase(format_chains.begin(), format_chains.end() - 1);
    }

    int format_idx;
    if (format_chains.size() == 1) {
      format_idx = 0;
    }
    else {
      vector<wstring> format_names;
      for (unsigned i = 0; i < format_chains.size(); i++) {
        format_names.push_back(format_chains[i].to_string());
      }
      format_idx = Far::menu(Far::get_msg(MSG_PLUGIN_NAME), format_names);
      if (format_idx == -1)
        return false;
    }

    if (!open(file_path, format_chains[format_idx]))
      return false;

    return true;
  }

public:
  Plugin() {
  }

  Plugin(const wstring& file_path) {
    if (!open_file(file_path, true))
      FAIL(E_ABORT);
  }

  Plugin(int open_from, INT_PTR item) {
    PanelInfo panel_info;
    if (!Far::get_panel_info(PANEL_ACTIVE, panel_info) || !Far::is_real_file_panel(panel_info))
      FAIL(E_ABORT);
    wstring name = Far::get_current_file_name(PANEL_ACTIVE);
    if (name == L"..")
      FAIL(E_ABORT);
    wstring dir = Far::get_panel_dir(PANEL_ACTIVE);
    wstring path = add_trailing_slash(dir) + name;
    if (!open_file(path, false))
      FAIL(E_ABORT);
  }

  void info(OpenPluginInfo* opi) {
    opi->StructSize = sizeof(OpenPluginInfo);
    opi->Flags = OPIF_USEFILTER | OPIF_USESORTGROUPS | OPIF_USEHIGHLIGHTING | OPIF_ADDDOTS;
    opi->CurDir = current_dir.c_str();
    panel_title = Far::get_msg(MSG_PLUGIN_NAME);
    if (is_open()) {
      panel_title += L":" + format_chain.to_string() + L":" + archive_file_info.cFileName;
      host_file = archive_file_info.cFileName;
    }
    opi->HostFile = host_file.c_str();
    opi->PanelTitle = panel_title.c_str();
  }

  void set_dir(const wstring& dir) {
    if (!is_open())
      FAIL(E_ABORT);
    wstring new_dir;
    if (dir == L"\\")
      new_dir.assign(dir);
    else if (dir == L"..")
      new_dir = extract_file_path(current_dir);
    else if (dir[0] == L'\\') // absolute path
      new_dir.assign(dir);
    else
      new_dir.assign(L"\\").append(add_trailing_slash(remove_path_root(current_dir))).append(dir);

    if (new_dir == L"\\")
      new_dir.clear();

    find_dir(new_dir);
    current_dir = new_dir;
  }

  void list(PluginPanelItem** panel_items, int* items_number) {
    if (!is_open())
      FAIL(E_ABORT);
    UInt32 dir_index = find_dir(current_dir);
    FileIndexRange dir_list = get_dir_list(dir_index);
    unsigned size = dir_list.second - dir_list.first;
    PluginPanelItem* items = new PluginPanelItem[size];
    memset(items, 0, size * sizeof(PluginPanelItem));
    try {
      unsigned idx = 0;
      for_each(dir_list.first, dir_list.second, [&] (UInt32 file_index) {
        const FileInfo& file_info = get_file_info(file_index);
        FAR_FIND_DATA& fdata = items[idx].FindData;
        fdata.dwFileAttributes = file_info.attr;
        fdata.ftCreationTime = file_info.ctime;
        fdata.ftLastAccessTime = file_info.atime;
        fdata.ftLastWriteTime = file_info.mtime;
        fdata.nFileSize = file_info.size;
        fdata.nPackSize = file_info.psize;
        fdata.lpwszFileName = file_info.name.c_str();
        items[idx].UserData = file_index;
        idx++;
      });
    }
    catch (...) {
      delete[] items;
      throw;
    }
    *panel_items = items;
    *items_number = size;
  }

  void get_files(const PluginPanelItem* panel_items, int items_number, int move, const wchar_t** dest_path, int op_mode) {
    if (items_number == 1 && wcscmp(panel_items[0].FindData.lpwszFileName, L"..") == 0) return;
    ExtractOptions options;
    options.dst_dir = *dest_path;
    options.ignore_errors = g_options.extract_ignore_errors;
    options.overwrite = static_cast<OverwriteOption>(g_options.extract_overwrite);
    options.move_enabled = updatable();
    options.move_files = move != 0 && options.move_enabled;
    options.show_dialog = (op_mode & (OPM_SILENT | OPM_FIND | OPM_VIEW | OPM_EDIT | OPM_QUICKVIEW)) == 0;
    options.use_tmp_files = (op_mode & (OPM_FIND | OPM_VIEW | OPM_QUICKVIEW)) != 0;
    if (op_mode & (OPM_FIND | OPM_QUICKVIEW))
      options.ignore_errors = true;
    if (!options.show_dialog)
      options.overwrite = ooOverwrite;
    if (options.show_dialog) {
      if (!extract_dialog(options)) FAIL(E_ABORT);
      if (options.dst_dir != *dest_path) {
        extract_dir = options.dst_dir;
        *dest_path = extract_dir.c_str();
      }
      g_options.extract_ignore_errors = options.ignore_errors;
      g_options.extract_overwrite = options.overwrite;
      g_options.save();
    }
    vector<UInt32> indices;
    UInt32 src_dir_index = find_dir(current_dir);
    indices.reserve(items_number);
    for (int i = 0; i < items_number; i++) {
      indices.push_back(panel_items[i].UserData);
    }

    ErrorLog error_log;
    extract(src_dir_index, indices, options, error_log);
    if (!error_log.empty()) {
      if (options.show_dialog)
        show_error_log(error_log);
    }
    else {
      if (options.move_files)
        Archive::delete_files(indices);
    }

    Far::update_panel(this, false);
  }

  void put_files(const PluginPanelItem* panel_items, int items_number, int move, const wchar_t* src_path, int op_mode) {
    if (items_number == 1 && wcscmp(panel_items[0].FindData.lpwszFileName, L"..") == 0) return;
    UpdateOptions options;
    bool new_arc = !is_open();
    if (new_arc) {
      wstring arc_dir;
      PanelInfo panel_info;
      if (Far::get_panel_info(PANEL_PASSIVE, panel_info) && Far::is_real_file_panel(panel_info))
        arc_dir = Far::get_panel_dir(PANEL_PASSIVE);
      else
        arc_dir = src_path;
      if (items_number == 1 || is_root_path(arc_dir))
        options.arc_path = add_trailing_slash(arc_dir) + panel_items[0].FindData.lpwszFileName;
      else
        options.arc_path = add_trailing_slash(arc_dir) + extract_file_name(src_path);
      options.arc_type = g_options.update_arc_type;
      options.arc_path += ArcAPI::get()->find_format(options.arc_type).default_extension();
    }
    else {
      options.arc_type = format_chain.back().name;
    }
    options.level = g_options.update_level;
    options.method = g_options.update_method;
    options.solid = g_options.update_solid;
    options.encrypt_header = g_options.update_encrypt_header;
    options.move_files = move != 0;
    options.show_dialog = (op_mode & (OPM_SILENT | OPM_FIND | OPM_VIEW | OPM_EDIT | OPM_QUICKVIEW)) == 0;
    if (options.show_dialog) {
      if (!update_dialog(new_arc, options)) FAIL(E_ABORT);
      g_options.update_arc_type = options.arc_type;
      g_options.update_level = options.level;
      g_options.update_method = options.method;
      g_options.update_solid = options.solid;
      g_options.update_encrypt_header = options.encrypt_header;
      g_options.save();
    }

    if (new_arc)
      create(src_path, panel_items, items_number, options);
    else
      update(src_path, panel_items, items_number, remove_path_root(current_dir), options);

    Far::update_panel(PANEL_ACTIVE, false);
    Far::update_panel(PANEL_PASSIVE, false);
  }

  void delete_files(const PluginPanelItem* panel_items, int items_number, int op_mode) {
    if (items_number == 1 && wcscmp(panel_items[0].FindData.lpwszFileName, L"..") == 0) return;

    bool show_dialog = (op_mode & (OPM_SILENT | OPM_FIND | OPM_VIEW | OPM_EDIT | OPM_QUICKVIEW)) == 0;
    if (show_dialog) {
      if (Far::message(Far::get_msg(MSG_PLUGIN_NAME) + L"\n" + Far::get_msg(MSG_DELETE_DLG_CONFIRM), 0, FMSG_MB_OKCANCEL) != 0)
        FAIL(E_ABORT);
    }

    vector<UInt32> indices;
    indices.reserve(items_number);
    for (int i = 0; i < items_number; i++) {
      indices.push_back(panel_items[i].UserData);
    }
    Archive::delete_files(indices);

    Far::update_panel(PANEL_ACTIVE, false);
    Far::update_panel(PANEL_PASSIVE, false);
  }
};

int WINAPI GetMinFarVersion(void) {
  return FARMANAGERVERSION;
}

int WINAPI GetMinFarVersionW(void) {
  return FARMANAGERVERSION;
}

void WINAPI SetStartupInfoW(const struct PluginStartupInfo *Info) {
  Far::init(Info);
  g_options.load();
}

void WINAPI GetPluginInfoW(struct PluginInfo *Info) {
  FAR_ERROR_HANDLER_BEGIN;
  static const wchar_t* plugin_menu[1];
  Info->StructSize = sizeof(PluginInfo);
  plugin_menu[0] = Far::msg_ptr(MSG_PLUGIN_NAME);
  Info->PluginMenuStrings = plugin_menu;
  Info->PluginMenuStringsNumber = ARRAYSIZE(plugin_menu);
  FAR_ERROR_HANDLER_END(return, return, false);
}

HANDLE WINAPI OpenPluginW(int OpenFrom,INT_PTR Item) {
  FAR_ERROR_HANDLER_BEGIN;
  return new Plugin(OpenFrom, Item);
  FAR_ERROR_HANDLER_END(return INVALID_HANDLE_VALUE, return INVALID_HANDLE_VALUE, false);
}

HANDLE WINAPI OpenFilePluginW(const wchar_t *Name,const unsigned char *Data,int DataSize,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN;
  if (Name == NULL)
    return new Plugin();
  else
    return new Plugin(Name);
  FAR_ERROR_HANDLER_END(return INVALID_HANDLE_VALUE, return INVALID_HANDLE_VALUE, (OpMode & (OPM_SILENT | OPM_FIND)) != 0);
}

void WINAPI ClosePluginW(HANDLE hPlugin) {
  FAR_ERROR_HANDLER_BEGIN;
  delete reinterpret_cast<Plugin*>(hPlugin);
  FAR_ERROR_HANDLER_END(return, return, true);
}

void WINAPI GetOpenPluginInfoW(HANDLE hPlugin,struct OpenPluginInfo *Info) {
  FAR_ERROR_HANDLER_BEGIN;
  reinterpret_cast<Plugin*>(hPlugin)->info(Info);
  FAR_ERROR_HANDLER_END(return, return, false);
}

int WINAPI SetDirectoryW(HANDLE hPlugin,const wchar_t *Dir,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN;
  reinterpret_cast<Plugin*>(hPlugin)->set_dir(Dir);
  return TRUE;
  FAR_ERROR_HANDLER_END(return FALSE, return FALSE, (OpMode & (OPM_SILENT | OPM_FIND)) != 0);
}

int WINAPI GetFindDataW(HANDLE hPlugin,struct PluginPanelItem **pPanelItem,int *pItemsNumber,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN;
  reinterpret_cast<Plugin*>(hPlugin)->list(pPanelItem, pItemsNumber);
  return TRUE;
  FAR_ERROR_HANDLER_END(return FALSE, return FALSE, (OpMode & (OPM_SILENT | OPM_FIND)) != 0);
}

void WINAPI FreeFindDataW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber) {
  FAR_ERROR_HANDLER_BEGIN;
  delete[] PanelItem;
  FAR_ERROR_HANDLER_END(return, return, false);
}

int WINAPI GetFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int Move,const wchar_t **DestPath,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN
  reinterpret_cast<Plugin*>(hPlugin)->get_files(PanelItem, ItemsNumber, Move, DestPath, OpMode);
  return 1;
  FAR_ERROR_HANDLER_END(return 0, return -1, (OpMode & (OPM_FIND | OPM_QUICKVIEW)) != 0);
}

int WINAPI PutFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int Move,const wchar_t *SrcPath,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN;
  reinterpret_cast<Plugin*>(hPlugin)->put_files(PanelItem, ItemsNumber, Move, SrcPath, OpMode);
  return 0;
  FAR_ERROR_HANDLER_END(return 0, return -1, (OpMode & OPM_FIND) != 0);
}

int WINAPI DeleteFilesW(HANDLE hPlugin,struct PluginPanelItem *PanelItem,int ItemsNumber,int OpMode) {
  FAR_ERROR_HANDLER_BEGIN;
  reinterpret_cast<Plugin*>(hPlugin)->delete_files(PanelItem, ItemsNumber, OpMode);
  return TRUE;
  FAR_ERROR_HANDLER_END(return FALSE, return FALSE, (OpMode & OPM_SILENT) != 0);
}

int WINAPI ProcessKeyW(HANDLE hPlugin,int Key,unsigned int ControlState) {
  FAR_ERROR_HANDLER_BEGIN;
  return FALSE;
  FAR_ERROR_HANDLER_END(return FALSE, return FALSE, false);
}

int WINAPI ConfigureW(int ItemNumber) {
  FAR_ERROR_HANDLER_BEGIN;
  return FALSE;
  FAR_ERROR_HANDLER_END(return FALSE, return FALSE, false);
}

void WINAPI ExitFARW() {
  ArcAPI::free();
}