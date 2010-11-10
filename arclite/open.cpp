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

  bool device_file;
  unsigned __int64 device_pos;
  unsigned __int64 device_size;

  void check_device_file() {
    device_pos = 0;
    device_file = false;
    if (size_nt(device_size))
      return;
    DWORD bytes_ret;
    PARTITION_INFORMATION part_info;
    BOOL res = DeviceIoControl(handle(), IOCTL_DISK_GET_PARTITION_INFO, nullptr, 0, &part_info, sizeof(PARTITION_INFORMATION), &bytes_ret, nullptr);
    if (res) {
      device_size = part_info.PartitionLength.QuadPart;
      device_file = true;
      return;
    }
    DISK_GEOMETRY disk_geometry;
    res = DeviceIoControl(handle(), IOCTL_DISK_GET_DRIVE_GEOMETRY, nullptr, 0, &disk_geometry, sizeof(DISK_GEOMETRY), &bytes_ret, nullptr);
    if (res) {
      device_size = disk_geometry.Cylinders.QuadPart * disk_geometry.TracksPerCylinder * disk_geometry.SectorsPerTrack * disk_geometry.BytesPerSector;
      device_file = true;
      return;
    }
  }
public:
  ArchiveOpenStream(const wstring& file_path): file_path(file_path) {
    open(file_path, FILE_READ_DATA | FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN);
    check_device_file();
  }

  UNKNOWN_IMPL_BEGIN
  UNKNOWN_IMPL_ITF(ISequentialInStream)
  UNKNOWN_IMPL_ITF(IInStream)
  UNKNOWN_IMPL_END

  STDMETHODIMP Read(void *data, UInt32 size, UInt32 *processedSize) {
    COM_ERROR_HANDLER_BEGIN
    unsigned size_read = read(data, size);
    if (device_file)
      device_pos += size_read;
    if (processedSize)
      *processedSize = size_read;
    return S_OK;
    COM_ERROR_HANDLER_END
  }

  STDMETHODIMP Seek(Int64 offset, UInt32 seekOrigin, UInt64 *newPosition) {
    COM_ERROR_HANDLER_BEGIN
    unsigned __int64 new_position;
    if (device_file) {
      switch (seekOrigin) {
      case STREAM_SEEK_SET:
        device_pos = offset; break;
      case STREAM_SEEK_CUR:
        device_pos += offset; break;
      case STREAM_SEEK_END:
        device_pos = device_size + offset; break;
      default:
        FAIL(E_INVALIDARG);
      }
      new_position = set_pos(device_pos, FILE_BEGIN);
    }
    else {
      new_position = set_pos(offset, translate_seek_method(seekOrigin));
    }
    if (newPosition)
      *newPosition = new_position;
    return S_OK;
    COM_ERROR_HANDLER_END
  }

  FindData get_info() {
    FindData file_info;
    memset(&file_info, 0, sizeof(file_info));
    wcscpy(file_info.cFileName, extract_file_name(file_path).c_str());
    BY_HANDLE_FILE_INFORMATION fi;
    if (get_info_nt(fi)) {
      file_info.dwFileAttributes = fi.dwFileAttributes;
      file_info.ftCreationTime = fi.ftCreationTime;
      file_info.ftLastAccessTime = fi.ftLastAccessTime;
      file_info.ftLastWriteTime = fi.ftLastWriteTime;
      file_info.nFileSizeLow = fi.nFileSizeLow;
      file_info.nFileSizeHigh = fi.nFileSizeHigh;
    }
    return file_info;
  }
};


class ArchiveOpener: public IArchiveOpenCallback, public IArchiveOpenVolumeCallback, public ICryptoGetTextPassword, public ComBase, public ProgressMonitor {
private:
  Archive& archive;
  FindData volume_file_info;

  UInt64 total_files;
  UInt64 total_bytes;
  UInt64 completed_files;
  UInt64 completed_bytes;

  virtual void do_update_ui() {
    const unsigned c_width = 60;
    wostringstream st;
    st << volume_file_info.cFileName << L'\n';
    st << completed_files << L" / " << total_files << L'\n';
    st << Far::get_progress_bar_str(c_width, completed_files, total_files) << L'\n';
    st << L"\x01\n";
    st << format_data_size(completed_bytes, get_size_suffixes()) << L" / " << format_data_size(total_bytes, get_size_suffixes()) << L'\n';
    st << Far::get_progress_bar_str(c_width, completed_bytes, total_bytes) << L'\n';
    progress_text = st.str();

    if (total_files)
      percent_done = calc_percent(completed_files, total_files);
    else
      percent_done = calc_percent(completed_bytes, total_bytes);
  }

public:
  ArchiveOpener(Archive& archive): ProgressMonitor(Far::get_msg(MSG_PROGRESS_OPEN)), archive(archive), volume_file_info(archive.arc_info), total_files(0), total_bytes(0), completed_files(0), completed_bytes(0) {
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
      prop = volume_file_info.cFileName; break;
    case kpidIsDir:
      prop = volume_file_info.is_dir(); break;
    case kpidSize:
      prop = volume_file_info.size(); break;
    case kpidAttrib:
      prop = static_cast<UInt32>(volume_file_info.dwFileAttributes); break;
    case kpidCTime:
      prop = volume_file_info.ftCreationTime; break;
    case kpidATime:
      prop = volume_file_info.ftLastAccessTime; break;
    case kpidMTime:
      prop = volume_file_info.ftLastWriteTime; break;
    }
    prop.detach(value);
    return S_OK;
    COM_ERROR_HANDLER_END
  }

  STDMETHODIMP GetStream(const wchar_t *name, IInStream **inStream) {
    COM_ERROR_HANDLER_BEGIN
    wstring file_path = add_trailing_slash(archive.arc_dir()) + name;
    FindData find_data;
    if (!get_find_data_nt(file_path, find_data))
      return S_FALSE;
    if (find_data.is_dir())
      return S_FALSE;
    archive.volume_names.insert(name);
    volume_file_info = find_data;
    ComObject<IInStream> file_stream(new ArchiveOpenStream(file_path));
    file_stream.detach(inStream);
    update_ui();
    return S_OK;
    COM_ERROR_HANDLER_END
  }

  STDMETHODIMP CryptoGetTextPassword(BSTR *password) {
    COM_ERROR_HANDLER_BEGIN
    if (archive.password.empty()) {
      ProgressSuspend ps(*this);
      if (!password_dialog(archive.password, archive.arc_path))
        FAIL(E_ABORT);
    }
    BStr(archive.password).detach(password);
    return S_OK;
    COM_ERROR_HANDLER_END
  }
};


bool Archive::open_sub_stream(IInStream** sub_stream, FindData& sub_arc_info) {
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
    wcscpy(sub_arc_info.cFileName, extract_file_name(prop.bstrVal).c_str());

  if (in_arc->GetProperty(main_subfile, kpidAttrib, prop.ref()) == S_OK && prop.is_uint())
    sub_arc_info.dwFileAttributes = static_cast<DWORD>(prop.get_uint());
  else
    sub_arc_info.dwFileAttributes = 0;

  if (in_arc->GetProperty(main_subfile, kpidSize, prop.ref()) == S_OK && prop.is_uint())
    sub_arc_info.set_size(prop.get_uint());
  else
    sub_arc_info.set_size(0);

  if (in_arc->GetProperty(main_subfile, kpidCTime, prop.ref()) == S_OK && prop.is_filetime())
    sub_arc_info.ftCreationTime = prop.get_filetime();
  else
    sub_arc_info.ftCreationTime = arc_info.ftCreationTime;
  if (in_arc->GetProperty(main_subfile, kpidMTime, prop.ref()) == S_OK && prop.is_filetime())
    sub_arc_info.ftLastWriteTime = prop.get_filetime();
  else
    sub_arc_info.ftLastWriteTime = arc_info.ftLastWriteTime;
  if (in_arc->GetProperty(main_subfile, kpidATime, prop.ref()) == S_OK && prop.is_filetime())
    sub_arc_info.ftLastAccessTime = prop.get_filetime();
  else
    sub_arc_info.ftLastAccessTime = arc_info.ftLastAccessTime;

  return true;
}

bool Archive::open(IInStream* in_stream) {
  CHECK_COM(in_stream->Seek(0, STREAM_SEEK_SET, nullptr));
  ComObject<IArchiveOpenCallback> opener(new ArchiveOpener(*this));
  const UInt64 max_check_start_position = max_check_size;
  HRESULT res;
  COM_ERROR_CHECK(res = in_arc->Open(in_stream, &max_check_start_position, opener));
  return res == S_OK;
}

void Archive::detect(const wstring& arc_path, bool all, vector<Archive>& archives) {
  size_t parent_idx = -1;
  if (!archives.empty())
    parent_idx = archives.size() - 1;

  ComObject<IInStream> stream;
  FindData arc_info;
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
  vector<ByteVector> signatures;
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
  ArcTypes types_by_ext = arc_formats.find_by_ext(extract_file_ext(arc_info.cFileName));
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
    if (arc_entry->type == c_iso) {
      iso_iter = arc_entry;
    }
    if (arc_entry->type == c_udf) {
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
