#include "msg.h"
#include "utils.hpp"
#include "sysutils.hpp"
#include "farutils.hpp"
#include "common.hpp"
#include "ui.hpp"
#include "msearch.hpp"
#include "archive.hpp"

class ArchiveOpenStream: public IInStream, private ComBase, private File {
private:
  wstring file_path;

public:
  ArchiveOpenStream(const wstring& file_path): file_path(file_path) {
    open(file_path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN);
  }

  UNKNOWN_IMPL_BEGIN
  UNKNOWN_IMPL_ITF(ISequentialInStream)
  UNKNOWN_IMPL_ITF(IInStream)
  UNKNOWN_IMPL_END

  STDMETHODIMP Read(void *data, UInt32 size, UInt32 *processedSize) {
    COM_ERROR_HANDLER_BEGIN
    ERROR_MESSAGE_BEGIN
    unsigned bytes_read = read(data, size);
    if (processedSize)
      *processedSize = bytes_read;
    return S_OK;
    ERROR_MESSAGE_END(file_path)
    COM_ERROR_HANDLER_END
  }

  STDMETHODIMP Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition) {
    COM_ERROR_HANDLER_BEGIN
    ERROR_MESSAGE_BEGIN
    unsigned __int64 new_position = set_pos(offset, translate_seek_method(seekOrigin));
    if (newPosition)
      *newPosition = new_position;
    return S_OK;
    ERROR_MESSAGE_END(file_path)
    COM_ERROR_HANDLER_END
  }

  FileInfo get_info() {
    FileInfo file_info;
    file_info.name = extract_file_name(file_path);
    BY_HANDLE_FILE_INFORMATION fi;
    if (get_info_nt(fi)) {
      file_info.attr = fi.dwFileAttributes;
      file_info.ctime = fi.ftCreationTime;
      file_info.atime = fi.ftLastAccessTime;
      file_info.mtime = fi.ftLastWriteTime;
      file_info.size = (static_cast<unsigned __int64>(fi.nFileSizeHigh) << 32) | fi.nFileSizeLow;
    }
    else {
      file_info.attr = 0;
      memset(&file_info.ctime, 0, sizeof(FILETIME));
      memset(&file_info.atime, 0, sizeof(FILETIME));
      memset(&file_info.mtime, 0, sizeof(FILETIME));
      file_info.size = 0;
    }
    return file_info;
  }
};


class ArchiveOpener: public IArchiveOpenCallback, public IArchiveOpenVolumeCallback, public ICryptoGetTextPassword, public ComBase, public ProgressMonitor {
private:
  Archive& archive;
  FileInfo volume_file_info;

  UInt64 total_files;
  UInt64 total_bytes;
  UInt64 completed_files;
  UInt64 completed_bytes;

  virtual void do_update_ui() {
    wostringstream st;
    st << Far::get_msg(MSG_PLUGIN_NAME) << L'\n';
    st << volume_file_info.name << L'\n';
    st << completed_files << L" / " << total_files << L'\n';
    st << Far::get_progress_bar_str(60, completed_files, total_files) << L'\n';
    st << L"\x01\n";
    st << format_data_size(completed_bytes, get_size_suffixes()) << L" / " << format_data_size(total_bytes, get_size_suffixes()) << L'\n';
    st << Far::get_progress_bar_str(60, completed_bytes, total_bytes) << L'\n';

    Far::message(st.str(), 0, FMSG_LEFTALIGN);

    unsigned percent;
    if (total_files)
      percent = calc_percent(completed_files, total_files);
    else
      percent = calc_percent(completed_bytes, total_bytes);

    Far::set_progress_state(TBPF_NORMAL);
    Far::set_progress_value(percent, 100);

    SetConsoleTitleW((L"{" + int_to_str(percent) + L"%} " + Far::get_msg(MSG_PROGRESS_OPEN)).c_str());
  }

public:
  ArchiveOpener(Archive& archive): archive(archive), volume_file_info(archive.arc_info), total_files(0), total_bytes(0), completed_files(0), completed_bytes(0) {
  }

  UNKNOWN_IMPL_BEGIN
  UNKNOWN_IMPL_ITF(IArchiveOpenCallback)
  UNKNOWN_IMPL_ITF(IArchiveOpenVolumeCallback)
  UNKNOWN_IMPL_ITF(ICryptoGetTextPassword)
  UNKNOWN_IMPL_END

  STDMETHODIMP SetTotal(const UInt64 *files, const UInt64 *bytes) {
    COM_ERROR_HANDLER_BEGIN
    if (files) total_files = *files;
    if (bytes) total_bytes = *bytes;
    update_ui();
    return S_OK;
    COM_ERROR_HANDLER_END
  }
  STDMETHODIMP SetCompleted(const UInt64 *files, const UInt64 *bytes) {
    COM_ERROR_HANDLER_BEGIN
    if (files) completed_files = *files;
    if (bytes) completed_bytes = *bytes;
    update_ui();
    return S_OK;
    COM_ERROR_HANDLER_END
  }

  STDMETHODIMP GetProperty(PROPID propID, PROPVARIANT *value) {
    COM_ERROR_HANDLER_BEGIN
    PropVariant prop;
    switch (propID) {
    case kpidName:
      prop = volume_file_info.name; break;
    case kpidIsDir:
      prop = volume_file_info.is_dir(); break;
    case kpidSize:
      prop = volume_file_info.size; break;
    case kpidAttrib:
      prop = static_cast<UInt32>(volume_file_info.attr); break;
    case kpidCTime:
      prop = volume_file_info.ctime; break;
    case kpidATime:
      prop = volume_file_info.atime; break;
    case kpidMTime:
      prop = volume_file_info.mtime; break;
    }
    prop.detach(value);
    return S_OK;
    COM_ERROR_HANDLER_END
  }

  STDMETHODIMP GetStream(const wchar_t *name, IInStream **inStream) {
    COM_ERROR_HANDLER_BEGIN
    wstring file_path = add_trailing_slash(archive.arc_dir()) + name;
    ERROR_MESSAGE_BEGIN
    FindData find_data;
    if (!get_find_data_nt(file_path, find_data))
      return S_FALSE;
    if (find_data.is_dir())
      return S_FALSE;
    archive.volume_names.insert(name);
    volume_file_info.convert(find_data);
    ComObject<IInStream> file_stream(new ArchiveOpenStream(file_path));
    file_stream.detach(inStream);
    update_ui();
    return S_OK;
    ERROR_MESSAGE_END(file_path)
    COM_ERROR_HANDLER_END
  }

  STDMETHODIMP CryptoGetTextPassword(BSTR *password) {
    COM_ERROR_HANDLER_BEGIN
    if (archive.password.empty()) {
      ProgressSuspend ps(*this);
      if (!password_dialog(archive.password))
        FAIL(E_ABORT);
    }
    BStr(archive.password).detach(password);
    return S_OK;
    COM_ERROR_HANDLER_END
  }
};


bool Archive::open_sub_stream(IInStream** sub_stream, FileInfo& sub_arc_info) {
  UInt32 main_subfile;
  PropVariant prop;
  if (in_arc->GetArchiveProperty(kpidMainSubfile, prop.ref()) != S_OK || prop.vt != VT_UI4)
    return false;
  main_subfile = prop.ulVal;

  UInt32 num_items;
  if (in_arc->GetNumberOfItems(&num_items) != S_OK || main_subfile >= num_items)
    return false;

  ComObject<IInArchiveGetStream> get_stream;
  if (in_arc->QueryInterface(IID_IInArchiveGetStream, reinterpret_cast<void**>(&get_stream)) != S_OK || !get_stream)
    return false;

  ComObject<ISequentialInStream> sub_seq_stream;
  if (get_stream->GetStream(main_subfile, &sub_seq_stream) != S_OK || !sub_seq_stream)
    return false;

  if (sub_seq_stream->QueryInterface(IID_IInStream, reinterpret_cast<void**>(sub_stream)) != S_OK || !sub_stream)
    return false;

  if (in_arc->GetProperty(main_subfile, kpidPath, prop.ref()) == S_OK && prop.vt == VT_BSTR)
    sub_arc_info.name = extract_file_name(prop.bstrVal);

  if (in_arc->GetProperty(main_subfile, kpidAttrib, prop.ref()) == S_OK && prop.is_uint())
    sub_arc_info.attr = static_cast<DWORD>(prop.get_uint());
  else
    sub_arc_info.attr = 0;

  if (in_arc->GetProperty(main_subfile, kpidSize, prop.ref()) == S_OK && prop.is_uint())
    sub_arc_info.size = prop.get_uint();
  else
    sub_arc_info.size = 0;

  if (in_arc->GetProperty(main_subfile, kpidCTime, prop.ref()) == S_OK && prop.is_filetime())
    sub_arc_info.ctime = prop.get_filetime();
  else
    sub_arc_info.ctime = arc_info.ctime;
  if (in_arc->GetProperty(main_subfile, kpidMTime, prop.ref()) == S_OK && prop.is_filetime())
    sub_arc_info.mtime = prop.get_filetime();
  else
    sub_arc_info.mtime = arc_info.mtime;
  if (in_arc->GetProperty(main_subfile, kpidATime, prop.ref()) == S_OK && prop.is_filetime())
    sub_arc_info.atime = prop.get_filetime();
  else
    sub_arc_info.atime = arc_info.atime;

  return true;
}

bool Archive::open(IInStream* in_stream) {
  CHECK_COM(in_stream->Seek(0, STREAM_SEEK_SET, nullptr));
  ComObject<IArchiveOpenCallback> opener(new ArchiveOpener(*this));
  const UInt64 max_check_start_position = max_check_size;
  HRESULT res = in_arc->Open(in_stream, &max_check_start_position, opener);
  COM_ERROR_CHECK(res);
  return res == S_OK;
}

void Archive::detect(const wstring& arc_path, bool all, vector<Archive>& archives) {
  unsigned parent_idx = -1;
  if (!archives.empty())
    parent_idx = archives.size() - 1;

  ComObject<IInStream> stream;
  FileInfo arc_info;
  memset(&arc_info, 0, sizeof(arc_info));
  if (parent_idx == -1) {
    ArchiveOpenStream* stream_impl = new ArchiveOpenStream(arc_path);
    stream = stream_impl;
    arc_info = stream_impl->get_info();
  }
  else {
    if (!archives[parent_idx].open_sub_stream(&stream, arc_info))
      return;
  }

  const ArcFormats& arc_formats = ArcAPI::formats();
  list<ArcEntry> arc_entries;
  set<ArcType> found_types;

  // 1. find formats by signature
  vector<string> signatures;
  signatures.reserve(arc_formats.size());
  vector<ArcType> sig_types;
  sig_types.reserve(arc_formats.size());
  for_each(arc_formats.begin(), arc_formats.end(), [&] (const pair<ArcType, ArcFormat>& arc_format) {
    if (!arc_format.second.start_signature.empty()) {
      sig_types.push_back(arc_format.first);
      signatures.push_back(arc_format.second.start_signature);
    }
  });

  Buffer<unsigned char> buffer(max_check_size);
  UInt32 size;
  CHECK_COM(stream->Read(buffer.data(), static_cast<UInt32>(buffer.size()), &size));

  vector<StrPos> sig_positions = msearch(buffer.data(), size, signatures);

  for_each(sig_positions.begin(), sig_positions.end(), [&] (const StrPos& sig_pos) {
    found_types.insert(sig_types[sig_pos.idx]);
    arc_entries.push_back(ArcEntry(sig_types[sig_pos.idx], sig_pos.pos));
  });

  // 2. find formats by file extension
  ArcTypes types_by_ext = arc_formats.find_by_ext(extract_file_ext(arc_info.name));
  for_each(types_by_ext.begin(), types_by_ext.end(), [&] (const ArcType& arc_type) {
    if (found_types.count(arc_type) == 0) {
      found_types.insert(arc_type);
      arc_entries.push_front(ArcEntry(arc_type, 0));
    }
  });

  // 3. all other formats
  for_each(arc_formats.begin(), arc_formats.end(), [&] (const pair<ArcType, ArcFormat>& arc_format) {
    if (found_types.count(arc_format.first) == 0) {
      arc_entries.push_back(ArcEntry(arc_format.first, 0));
    }
  });

  // special case: UDF must go before ISO
  list<ArcEntry>::iterator iso_iter = arc_entries.end();
  for (list<ArcEntry>::iterator arc_entry = arc_entries.begin(); arc_entry != arc_entries.end(); arc_entry++) {
    if (arc_entry->type == c_guid_iso) {
      iso_iter = arc_entry;
    }
    if (arc_entry->type == c_guid_udf) {
      if (iso_iter != arc_entries.end()) {
        arc_entries.insert(iso_iter, *arc_entry);
        arc_entries.erase(arc_entry);
      }
      break;
    }
  }

  for (list<ArcEntry>::const_iterator arc_entry = arc_entries.begin(); arc_entry != arc_entries.end(); arc_entry++) {
    Archive archive;
    archive.arc_path = arc_path;
    archive.arc_info = arc_info;
    if (parent_idx != -1)
      archive.volume_names = archives[parent_idx].volume_names;
    ArcAPI::create_in_archive(arc_entry->type, &archive.in_arc);
    if (archive.open(stream)) {
      if (parent_idx != -1)
        archive.arc_chain.assign(archives[parent_idx].arc_chain.begin(), archives[parent_idx].arc_chain.end());
      archive.arc_chain.push_back(*arc_entry);
      archives.push_back(archive);
      detect(arc_path, all, archives);
      if (!all) break;
    }
  }
}

vector<Archive> Archive::detect(const wstring& file_path, bool all) {
  vector<Archive> archives;
  detect(file_path, all, archives);
  if (!all && !archives.empty())
    archives.erase(archives.begin(), archives.end() - 1);
  return archives;
}

void Archive::reopen() {
  assert(!in_arc);
  volume_names.clear();
  ArchiveOpenStream* stream_impl = new ArchiveOpenStream(arc_path);
  ComObject<IInStream> stream(stream_impl);
  arc_info = stream_impl->get_info();
  ArcChain::const_iterator arc_entry = arc_chain.begin();
  ArcAPI::create_in_archive(arc_entry->type, &in_arc);
  if (!open(stream))
    FAIL(E_FAIL);
  arc_entry++;
  while (arc_entry != arc_chain.end()) {
    ComObject<IInStream> sub_stream;
    CHECK(open_sub_stream(&sub_stream, arc_info));
    ArcAPI::create_in_archive(arc_entry->type, &in_arc);
    if (!open(sub_stream))
      FAIL(E_FAIL);
    arc_entry++;
  }
}

void Archive::close() {
  if (in_arc) {
    in_arc->Close();
    in_arc.Release();
  }
  file_list.clear();
  file_list_index.clear();
}
