#include "farapi_config.h"

#define _ERROR_WINDOWS
#include "error.h"

#include "msg.h"

#include "utils.h"
#include "dlgapi.h"
#include "filever.h"

extern struct PluginStartupInfo g_far;
extern Array<unsigned char> g_colors;

struct NameValue {
  UnicodeString name;
  UnicodeString value;
  NameValue(const UnicodeString& name, const UnicodeString& value): name(name), value(value) {
  }
};

struct VarVerInfo {
  WORD lang;
  WORD code_page;
  ObjectArray<NameValue> strings;
};

struct VersionInfo {
  ObjectArray<NameValue> fixed;
  ObjectArray<VarVerInfo> var;
  ObjectArray<NameValue> sig;
};

ObjectArray<NameValue> get_signature_info(const UnicodeString& file_name);

VersionInfo get_version_info(const UnicodeString& file_name) {
  VersionInfo ver_info;

  DWORD dw_handle;
  DWORD ver_size = GetFileVersionInfoSizeW(file_name.data(), &dw_handle);
  if (ver_size) {
    Array<unsigned char> ver_block;
    CHECK_SYS(GetFileVersionInfoW(file_name.data(), dw_handle, ver_size, ver_block.buf(ver_size)));
    ver_block.set_size(ver_size);
    VS_FIXEDFILEINFO* fixed_file_info;
    UINT len;
    CHECK_SYS(VerQueryValueW(ver_block.data(), L"\\", reinterpret_cast<LPVOID*>(&fixed_file_info), &len));

    UnicodeString file_version = UnicodeString::format(L"%u.%u.%u.%u", HIWORD(fixed_file_info->dwFileVersionMS), LOWORD(fixed_file_info->dwFileVersionMS), HIWORD(fixed_file_info->dwFileVersionLS), LOWORD(fixed_file_info->dwFileVersionLS));
    if (file_version.size()) ver_info.fixed += NameValue(far_get_msg(MSG_FILE_VER_FIXED_FILE_VERSION), file_version);
    UnicodeString product_version = UnicodeString::format(L"%u.%u.%u.%u", HIWORD(fixed_file_info->dwProductVersionMS), LOWORD(fixed_file_info->dwProductVersionMS), HIWORD(fixed_file_info->dwProductVersionLS), LOWORD(fixed_file_info->dwProductVersionLS));
    if (product_version.size()) ver_info.fixed += NameValue(far_get_msg(MSG_FILE_VER_FIXED_PRODUCT_VERSION), product_version);

    UnicodeString file_flags;
    #define FILE_FLAG(name) \
    if (fixed_file_info->dwFileFlags & fixed_file_info->dwFileFlagsMask & name) { \
      if (file_flags.size()) file_flags += L", "; \
      file_flags += L#name; \
    }
    FILE_FLAG(VS_FF_DEBUG)
    FILE_FLAG(VS_FF_INFOINFERRED)
    FILE_FLAG(VS_FF_PATCHED)
    FILE_FLAG(VS_FF_PRERELEASE)
    FILE_FLAG(VS_FF_PRIVATEBUILD)
    FILE_FLAG(VS_FF_SPECIALBUILD)
    #undef FILE_FLAG
    if (file_flags.size()) ver_info.fixed += NameValue(far_get_msg(MSG_FILE_VER_FILE_FLAGS), file_flags);

    UnicodeString file_os;
    #define OS_FLAG(name) \
    if (fixed_file_info->dwFileOS & name) { \
      if (file_os.size()) file_os += L", "; \
      file_os += L#name; \
    }
    OS_FLAG(VOS_DOS)
    OS_FLAG(VOS_NT)
    OS_FLAG(VOS__WINDOWS16)
    OS_FLAG(VOS__WINDOWS32)
    OS_FLAG(VOS_OS216)
    OS_FLAG(VOS_OS232)
    OS_FLAG(VOS__PM16)
    OS_FLAG(VOS__PM32)
    OS_FLAG(VOS_UNKNOWN)
    #undef OS_FLAG
    if (file_os.size()) ver_info.fixed += NameValue(far_get_msg(MSG_FILE_VER_FILE_OS), file_os);

    UnicodeString file_type;
    #define FILE_TYPE(name) \
    if (fixed_file_info->dwFileType == name) { \
      file_type = L#name; \
    }
    FILE_TYPE(VFT_UNKNOWN)
    FILE_TYPE(VFT_APP)
    FILE_TYPE(VFT_DLL)
    FILE_TYPE(VFT_DRV)
    FILE_TYPE(VFT_FONT)
    FILE_TYPE(VFT_VXD)
    FILE_TYPE(VFT_STATIC_LIB)
    #undef FILE_TYPE
    if (file_type.size()) ver_info.fixed += NameValue(far_get_msg(MSG_FILE_VER_FILE_TYPE), file_type);

    UnicodeString file_sub_type;
    #define FILE_SUB_TYPE(name) \
    if (fixed_file_info->dwFileSubtype == name) { \
      file_sub_type = L#name; \
    }
    if (fixed_file_info->dwFileType == VFT_DRV) {
      FILE_SUB_TYPE(VFT2_UNKNOWN)
      FILE_SUB_TYPE(VFT2_DRV_COMM)
      FILE_SUB_TYPE(VFT2_DRV_PRINTER)
      FILE_SUB_TYPE(VFT2_DRV_KEYBOARD)
      FILE_SUB_TYPE(VFT2_DRV_LANGUAGE)
      FILE_SUB_TYPE(VFT2_DRV_DISPLAY)
      FILE_SUB_TYPE(VFT2_DRV_MOUSE)
      FILE_SUB_TYPE(VFT2_DRV_NETWORK)
      FILE_SUB_TYPE(VFT2_DRV_SYSTEM)
      FILE_SUB_TYPE(VFT2_DRV_INSTALLABLE)
      FILE_SUB_TYPE(VFT2_DRV_SOUND)
      FILE_SUB_TYPE(VFT2_DRV_VERSIONED_PRINTER)
    }
    if (fixed_file_info->dwFileType == VFT_FONT) {
      FILE_SUB_TYPE(VFT2_UNKNOWN)
      FILE_SUB_TYPE(VFT2_FONT_RASTER)
      FILE_SUB_TYPE(VFT2_FONT_VECTOR)
      FILE_SUB_TYPE(VFT2_FONT_TRUETYPE)
    }
    #undef FILE_SUB_TYPE
    if (file_sub_type.size()) ver_info.fixed += NameValue(far_get_msg(MSG_FILE_VER_FILE_SUB_TYPE), file_sub_type);

    const wchar_t* c_str_names[] = { L"Comments", L"CompanyName", L"FileDescription", L"FileVersion", L"InternalName", L"LegalCopyright", L"LegalTrademarks", L"OriginalFilename", L"ProductName", L"ProductVersion", L"PrivateBuild", L"SpecialBuild" };
    const unsigned c_str_labels[] = { MSG_FILE_VER_COMMENTS, MSG_FILE_VER_COMPANY_NAME, MSG_FILE_VER_FILE_DESCRIPTION, MSG_FILE_VER_FILE_VERSION, MSG_FILE_VER_INTERNAL_NAME, MSG_FILE_VER_LEGAL_COPYRIGHT, MSG_FILE_VER_LEGAL_TRADEMARKS, MSG_FILE_VER_ORIGINAL_FILE_NAME, MSG_FILE_VER_PRODUCT_NAME, MSG_FILE_VER_PRODUCT_VERSION, MSG_FILE_VER_PRIVATE_BUILD, MSG_FILE_VER_SPECIAL_BUILD };
    WORD* lang_cp_array;
    if (VerQueryValueW(ver_block.data(), L"\\VarFileInfo\\Translation", reinterpret_cast<LPVOID*>(&lang_cp_array), &len) && len && (len / sizeof(WORD) % 2 == 0)) {
      unsigned lang_cp_cnt = len / sizeof(WORD);
      for (unsigned i = 0; i < lang_cp_cnt; i += 2) {
        VarVerInfo var_ver_info;
        var_ver_info.lang = lang_cp_array[i];
        var_ver_info.code_page = lang_cp_array[i + 1];

        LCID lcid = GetThreadLocale();
        CLEAN(LCID, lcid, SetThreadLocale(lcid));
        try {
          CHECK_SYS(SetThreadLocale(MAKELCID(var_ver_info.lang, SORT_DEFAULT)));
          DWORD dw_handle;
          DWORD ver_size = GetFileVersionInfoSizeW(file_name.data(), &dw_handle);
          CHECK_SYS(ver_size);
          Array<unsigned char> ver_block;
          CHECK_SYS(GetFileVersionInfoW(file_name.data(), dw_handle, ver_size, ver_block.buf(ver_size)));
          ver_block.set_size(ver_size);

          for (unsigned j = 0; j < ARRAYSIZE(c_str_names); j++) {
            UnicodeString sub_block_name;
            sub_block_name.copy_fmt(L"\\StringFileInfo\\%Hx%Hx\\%s", var_ver_info.lang, var_ver_info.code_page, c_str_names[j]);
            wchar_t* str_value;
            if (VerQueryValueW(ver_block.data(), sub_block_name.data(), reinterpret_cast<LPVOID*>(&str_value), &len) && len) {
              UnicodeString value = UnicodeString(str_value, len).strip();
              if (value.size())
                var_ver_info.strings += NameValue(far_get_msg(c_str_labels[j]), value);
            }
          }
          if (var_ver_info.strings.size()) ver_info.var += var_ver_info;
        }
        catch (...) {
        }
      }
    }
  }

  LOADED_IMAGE* exec_image = ImageLoad(unicode_to_ansi(file_name).data(), NULL);
  if (exec_image) {
    CLEAN(LOADED_IMAGE*, exec_image, ImageUnload(exec_image));
    UnicodeString machine;
    #define MACHINE(name) \
    if (exec_image->FileHeader->FileHeader.Machine == IMAGE_FILE_MACHINE_##name) { \
      machine = L#name; \
    }
    MACHINE(I386)
    MACHINE(AMD64)
    MACHINE(IA64)
    #undef MACHINE
    if (machine.size()) ver_info.fixed += NameValue(far_get_msg(MSG_FILE_VER_MACHINE), machine);

    ver_info.fixed += NameValue(far_get_msg(MSG_FILE_VER_OS_VERSION), UnicodeString::format(L"%Hu.%Hu", exec_image->FileHeader->OptionalHeader.MajorOperatingSystemVersion, exec_image->FileHeader->OptionalHeader.MinorOperatingSystemVersion));

    UnicodeString sub_system;
    #define SUB_SYSTEM(name) \
    if (exec_image->FileHeader->OptionalHeader.Subsystem == IMAGE_SUBSYSTEM_##name) { \
      sub_system = L#name; \
    }
    SUB_SYSTEM(UNKNOWN)
    SUB_SYSTEM(NATIVE)
    SUB_SYSTEM(WINDOWS_GUI)
    SUB_SYSTEM(WINDOWS_CUI)
    SUB_SYSTEM(OS2_CUI)
    SUB_SYSTEM(POSIX_CUI)
    SUB_SYSTEM(WINDOWS_CE_GUI)
    SUB_SYSTEM(EFI_APPLICATION)
    SUB_SYSTEM(EFI_BOOT_SERVICE_DRIVER)
    SUB_SYSTEM(EFI_RUNTIME_DRIVER)
    SUB_SYSTEM(EFI_ROM)
    SUB_SYSTEM(XBOX)
    SUB_SYSTEM(WINDOWS_BOOT_APPLICATION)
    #undef SUB_SYSTEM
    if (sub_system.size()) ver_info.fixed += NameValue(far_get_msg(MSG_FILE_VER_SUBSYSTEM), sub_system);

    UnicodeString image_chars;
    #define IMAGE_CHAR_FLAG(name) \
    if (exec_image->FileHeader->FileHeader.Characteristics & IMAGE_FILE_##name) { \
      if (image_chars.size()) image_chars += L", "; \
      image_chars += L#name; \
    }
    IMAGE_CHAR_FLAG(RELOCS_STRIPPED)
    IMAGE_CHAR_FLAG(EXECUTABLE_IMAGE)
    IMAGE_CHAR_FLAG(LINE_NUMS_STRIPPED)
    IMAGE_CHAR_FLAG(LOCAL_SYMS_STRIPPED)
    IMAGE_CHAR_FLAG(AGGRESIVE_WS_TRIM)
    IMAGE_CHAR_FLAG(LARGE_ADDRESS_AWARE)
    IMAGE_CHAR_FLAG(BYTES_REVERSED_LO)
    IMAGE_CHAR_FLAG(32BIT_MACHINE)
    IMAGE_CHAR_FLAG(DEBUG_STRIPPED)
    IMAGE_CHAR_FLAG(REMOVABLE_RUN_FROM_SWAP)
    IMAGE_CHAR_FLAG(NET_RUN_FROM_SWAP)
    IMAGE_CHAR_FLAG(SYSTEM)
    IMAGE_CHAR_FLAG(DLL)
    IMAGE_CHAR_FLAG(UP_SYSTEM_ONLY)
    IMAGE_CHAR_FLAG(BYTES_REVERSED_HI)
    #undef IMAGE_CHAR_FLAG
    if (image_chars.size()) ver_info.fixed += NameValue(far_get_msg(MSG_FILE_VER_IMAGE_CHARS), image_chars);

    UnicodeString dll_chars;
    #define DLL_CHAR_FLAG(name) \
    if (exec_image->FileHeader->OptionalHeader.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_##name) { \
      if (dll_chars.size()) dll_chars += L", "; \
      dll_chars += L#name; \
    }
    DLL_CHAR_FLAG(DYNAMIC_BASE)
    DLL_CHAR_FLAG(FORCE_INTEGRITY)
    DLL_CHAR_FLAG(NX_COMPAT)
    DLL_CHAR_FLAG(NO_ISOLATION)
    DLL_CHAR_FLAG(NO_SEH)
    DLL_CHAR_FLAG(NO_BIND)
    DLL_CHAR_FLAG(WDM_DRIVER)
    DLL_CHAR_FLAG(TERMINAL_SERVER_AWARE)
    #undef DLL_CHAR_FLAG
    if (dll_chars.size()) ver_info.fixed += NameValue(far_get_msg(MSG_FILE_VER_DLL_CHARS), dll_chars);
  }

  ver_info.sig = get_signature_info(file_name);

  return ver_info;
}


struct CertificateInfo {
  UnicodeString issuer;
  UnicodeString subject;
};

CertificateInfo get_certificate_info(HCERTSTORE h_cert_store, const CMSG_SIGNER_INFO* signer_info) {
  CertificateInfo certificate_info;
  // Search for the signer certificate in the temporary certificate store.
  CERT_INFO cert_info;
  cert_info.Issuer = signer_info->Issuer;
  cert_info.SerialNumber = signer_info->SerialNumber;

  struct CertContext {
    PCCERT_CONTEXT cert_context;
    CertContext(): cert_context(NULL) {
    }
    ~CertContext() {
      if (cert_context)
        VERIFY(CertFreeCertificateContext(cert_context));
    }
  };
  CertContext object;
  object.cert_context = CertFindCertificateInStore(h_cert_store, X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, 0, CERT_FIND_SUBJECT_CERT, &cert_info, NULL);
  if (object.cert_context) {
    // Get Issuer name.
    DWORD issuer_size = CertGetNameStringW(object.cert_context, CERT_NAME_FRIENDLY_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, NULL, NULL, 0);
    issuer_size = CertGetNameStringW(object.cert_context, CERT_NAME_FRIENDLY_DISPLAY_TYPE, CERT_NAME_ISSUER_FLAG, NULL, certificate_info.issuer.buf(issuer_size), issuer_size);
    if (issuer_size) issuer_size -= 1;
    certificate_info.issuer.set_size(issuer_size);
    // Get Subject name.
    DWORD subject_size = CertGetNameStringW(object.cert_context, CERT_NAME_FRIENDLY_DISPLAY_TYPE, 0, NULL, NULL, 0);
    subject_size = CertGetNameStringW(object.cert_context, CERT_NAME_FRIENDLY_DISPLAY_TYPE, 0, NULL, certificate_info.subject.buf(subject_size), subject_size);
    if (subject_size) subject_size -= 1;
    certificate_info.subject.set_size(subject_size);
  }
  return certificate_info;
}

ObjectArray<NameValue> get_signature_info(const UnicodeString& file_name) {
  ObjectArray<NameValue> sig_info;

  struct CryptObject {
    HCERTSTORE h_cert_store;
    HCRYPTMSG h_crypt_msg;
    CryptObject(): h_cert_store(NULL), h_crypt_msg(NULL) {
    }
    ~CryptObject() {
      if (h_cert_store)
        VERIFY(CertCloseStore(h_cert_store, CERT_CLOSE_STORE_FORCE_FLAG));
      if (h_crypt_msg)
        VERIFY(CryptMsgClose(h_crypt_msg));
    }
  };

  CryptObject crypt_object;
  DWORD encoding_type, content_type, format_type;
  // Get message handle and store handle from the signed file.
  if (!CryptQueryObject(CERT_QUERY_OBJECT_FILE, file_name.data(), CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED, CERT_QUERY_FORMAT_FLAG_BINARY, 0, &encoding_type, &content_type, &format_type, &crypt_object.h_cert_store, &crypt_object.h_crypt_msg, NULL)) return sig_info;

  // Get signer information size.
  DWORD signer_info_size;
  if (!CryptMsgGetParam(crypt_object.h_crypt_msg, CMSG_SIGNER_INFO_PARAM, 0, NULL, &signer_info_size)) return sig_info;
  Array<unsigned char> signer_info_buffer;
  if (!CryptMsgGetParam(crypt_object.h_crypt_msg, CMSG_SIGNER_INFO_PARAM, 0, signer_info_buffer.buf(signer_info_size), &signer_info_size)) return sig_info;
  signer_info_buffer.set_size(signer_info_size);
  const CMSG_SIGNER_INFO* signer_info = reinterpret_cast<const CMSG_SIGNER_INFO*>(signer_info_buffer.data());

  // Loop through authenticated attributes and find SPC_SP_OPUS_INFO_OBJID OID.
  for (DWORD i = 0; i < signer_info->AuthAttrs.cAttr; i++) {
    DWORD opus_info_size;
    if (strcmp(SPC_SP_OPUS_INFO_OBJID, signer_info->AuthAttrs.rgAttr[i].pszObjId) == 0) {
      CRYPT_ATTR_BLOB& value = signer_info->AuthAttrs.rgAttr[i].rgValue[0];
      if (!CryptDecodeObject(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, SPC_SP_OPUS_INFO_OBJID, value.pbData, value.cbData, 0, NULL, &opus_info_size)) break;
      Array<unsigned char> opus_info_buffer;
      if (!CryptDecodeObject(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, SPC_SP_OPUS_INFO_OBJID, value.pbData, value.cbData, 0, opus_info_buffer.buf(opus_info_size), &opus_info_size)) break;
      opus_info_buffer.set_size(opus_info_size);
      const SPC_SP_OPUS_INFO* opus_info = reinterpret_cast<const SPC_SP_OPUS_INFO*>(opus_info_buffer.data());

      if (opus_info->pwszProgramName) {
        sig_info.add(NameValue(far_get_msg(MSG_FILE_VER_PROGRAM_NAME), opus_info->pwszProgramName));
      }
      if (opus_info->pPublisherInfo) {
        switch (opus_info->pPublisherInfo->dwLinkChoice) {
        case SPC_URL_LINK_CHOICE:
          sig_info.add(NameValue(far_get_msg(MSG_FILE_VER_PUBLISHER_INFO), opus_info->pPublisherInfo->pwszUrl)); break;
        case SPC_FILE_LINK_CHOICE:
          sig_info.add(NameValue(far_get_msg(MSG_FILE_VER_PUBLISHER_INFO), opus_info->pPublisherInfo->pwszFile)); break;
        }
      }
      if (opus_info->pMoreInfo) {
        switch (opus_info->pMoreInfo->dwLinkChoice) {
        case SPC_URL_LINK_CHOICE:
          sig_info.add(NameValue(far_get_msg(MSG_FILE_VER_MORE_INFO), opus_info->pMoreInfo->pwszUrl)); break;
        case SPC_FILE_LINK_CHOICE:
          sig_info.add(NameValue(far_get_msg(MSG_FILE_VER_MORE_INFO), opus_info->pMoreInfo->pwszFile)); break;
        }
      }

      break;
    }
  }

  CertificateInfo signer_certificate = get_certificate_info(crypt_object.h_cert_store, signer_info);
  if (signer_certificate.issuer.size())
    sig_info.add(NameValue(far_get_msg(MSG_FILE_VER_SIGNER_ISSUER), signer_certificate.issuer));
  if (signer_certificate.subject.size())
    sig_info.add(NameValue(far_get_msg(MSG_FILE_VER_SIGNER_SUBJECT), signer_certificate.subject));

  // Timestamp
  for (DWORD i = 0; i < signer_info->UnauthAttrs.cAttr; i++) {
    if (strcmp(signer_info->UnauthAttrs.rgAttr[i].pszObjId, szOID_RSA_counterSign) == 0) {
      CRYPT_ATTR_BLOB& value = signer_info->UnauthAttrs.rgAttr[i].rgValue[0];
      DWORD counter_signer_info_size;
      if (!CryptDecodeObject(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, PKCS7_SIGNER_INFO, value.pbData, value.cbData, 0, NULL, &counter_signer_info_size)) break;
      Array<unsigned char> counter_signer_info_buffer;
      if (!CryptDecodeObject(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, PKCS7_SIGNER_INFO, value.pbData, value.cbData, 0, counter_signer_info_buffer.buf(counter_signer_info_size), &counter_signer_info_size)) break;
      counter_signer_info_buffer.set_size(counter_signer_info_size);
      const CMSG_SIGNER_INFO* counter_signer_info = reinterpret_cast<const CMSG_SIGNER_INFO*>(counter_signer_info_buffer.data());

      for (DWORD i = 0; i < counter_signer_info->AuthAttrs.cAttr; i++) {
        if (strcmp(counter_signer_info->AuthAttrs.rgAttr[i].pszObjId, szOID_RSA_signingTime) == 0) {
          CRYPT_ATTR_BLOB& value = counter_signer_info->AuthAttrs.rgAttr[i].rgValue[0];
          FILETIME ft, local_ft;
          SYSTEMTIME st;
          DWORD ft_size = sizeof(ft);
          if (!CryptDecodeObject(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, szOID_RSA_signingTime, value.pbData, value.cbData, 0, &ft, &ft_size)) break;
          FileTimeToLocalFileTime(&ft, &local_ft);
          FileTimeToSystemTime(&local_ft, &st);
          int date_size = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, NULL, 0);
          if (date_size == 0) break;
          UnicodeString date;
          date_size = GetDateFormatW(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date.buf(date_size), date_size);
          if (date_size) date_size -= 1;
          date.set_size(date_size);
          int time_size = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, NULL, NULL, 0);
          if (time_size == 0) break;
          UnicodeString time;
          time_size = GetTimeFormatW(LOCALE_USER_DEFAULT, 0, &st, NULL, time.buf(time_size), time_size);
          if (time_size) time_size -= 1;
          time.set_size(time_size);
          sig_info.add(NameValue(far_get_msg(MSG_FILE_VER_TIMESTAMP), date + L' ' + time));
          break;
        }
      }

      CertificateInfo timestamp_certificate = get_certificate_info(crypt_object.h_cert_store, counter_signer_info);
      if (timestamp_certificate.issuer.size())
        sig_info.add(NameValue(far_get_msg(MSG_FILE_VER_TIMESTAMP_ISSUER), timestamp_certificate.issuer));
      if (timestamp_certificate.subject.size())
        sig_info.add(NameValue(far_get_msg(MSG_FILE_VER_TIMESTAMP_SUBJECT), timestamp_certificate.subject));

      break;
    }
  }

  return sig_info;
}

LONG verify_signature(const UnicodeString& file_name) {
  WINTRUST_FILE_INFO wintrust_file_info;
  memset(&wintrust_file_info, 0, sizeof(wintrust_file_info));
  wintrust_file_info.cbStruct = sizeof(WINTRUST_FILE_INFO);
  wintrust_file_info.pcwszFilePath = file_name.data();
  wintrust_file_info.hFile = NULL;
  wintrust_file_info.pgKnownSubject = NULL;

  WINTRUST_DATA wintrust_data;
  memset(&wintrust_data, 0, sizeof(wintrust_data));
  wintrust_data.cbStruct = sizeof(wintrust_data);
  wintrust_data.pPolicyCallbackData = NULL;
  wintrust_data.pSIPClientData = NULL;
  wintrust_data.dwUIChoice = WTD_UI_NONE;
  wintrust_data.fdwRevocationChecks = WTD_REVOKE_NONE;
  wintrust_data.dwUnionChoice = WTD_CHOICE_FILE;
  wintrust_data.pFile = &wintrust_file_info;
  wintrust_data.dwStateAction = WTD_STATEACTION_IGNORE;
  wintrust_data.pwszURLReference = NULL;
  wintrust_data.dwProvFlags = WTD_SAFER_FLAG;
  wintrust_data.dwUIContext = WTD_UICONTEXT_EXECUTE;

  GUID action_id = WINTRUST_ACTION_GENERIC_VERIFY_V2;

  return WinVerifyTrust(static_cast<HWND>(INVALID_HANDLE_VALUE), &action_id, &wintrust_data);
}


struct FileVersionDialogData {
  UnicodeString file_name;
  int lang_cp_ctrl_id;
  unsigned var_cnt;
  int verify_ctrl_id;
  int verify_result_ctrl_id;
};

LONG_PTR WINAPI file_version_dialog_proc(HANDLE h_dlg, int msg, int param1, LONG_PTR param2) {
  BEGIN_ERROR_HANDLER;
  FarDialog* dlg = FarDialog::get_dlg(h_dlg);
  const FileVersionDialogData* dlg_data = reinterpret_cast<const FileVersionDialogData*>(dlg->get_dlg_data(0));
  const VersionInfo* version_info = reinterpret_cast<const VersionInfo*>(dlg->get_dlg_data(1));
  if (msg == DN_INITDIALOG) {
    file_version_dialog_proc(h_dlg, DN_EDITCHANGE, dlg_data->lang_cp_ctrl_id, 0);
    return FALSE;
  }
  else if (msg == DN_EDITCHANGE && param1 == dlg_data->lang_cp_ctrl_id) {
    if (version_info->var.size()) {
      unsigned lang_cp_idx;
      if (version_info->var.size() == 1) lang_cp_idx = 0;
      else lang_cp_idx = dlg->get_list_pos(dlg_data->lang_cp_ctrl_id);

      int ctrl_id = dlg_data->lang_cp_ctrl_id + 1;
      for (unsigned i = 0; i < version_info->var[lang_cp_idx].strings.size(); i++) {
        dlg->set_text(ctrl_id, version_info->var[lang_cp_idx].strings[i].name);
        g_far.SendDlgMessage(h_dlg, DM_SHOWITEM, ctrl_id, 1);
        ctrl_id++;
        dlg->set_text(ctrl_id, version_info->var[lang_cp_idx].strings[i].value);
        g_far.SendDlgMessage(h_dlg, DM_SHOWITEM, ctrl_id, 1);
        ctrl_id++;
      }
      for (unsigned i = version_info->var[lang_cp_idx].strings.size(); i < dlg_data->var_cnt; i++) {
        g_far.SendDlgMessage(h_dlg, DM_SHOWITEM, ctrl_id, 0);
        ctrl_id++;
        g_far.SendDlgMessage(h_dlg, DM_SHOWITEM, ctrl_id, 0);
        ctrl_id++;
      }
    }

    FarDialogItem dlg_item;
    for (unsigned ctrl_id = 0; g_far.SendDlgMessage(h_dlg, DM_GETDLGITEMSHORT, ctrl_id, reinterpret_cast<LONG_PTR>(&dlg_item)); ctrl_id++) {
      if (dlg_item.Type == DI_EDIT || dlg_item.Type == DI_COMBOBOX) {
        g_far.SendDlgMessage(h_dlg, DM_EDITUNCHANGEDFLAG, ctrl_id, 0);
        EditorSetPosition esp = { 0 };
        g_far.SendDlgMessage(h_dlg, DM_SETEDITPOSITION, ctrl_id, reinterpret_cast<LONG_PTR>(&esp));
      }
    }

    return TRUE;
  }
  else if (msg == DN_BTNCLICK && param1 == dlg_data->verify_ctrl_id) {
    dlg->set_text(dlg_data->verify_result_ctrl_id, far_get_msg(MSG_FILE_VER_VERIFY_IN_PROGRESS));
    LONG result = verify_signature(dlg_data->file_name);
    unsigned char result_color;
    UnicodeString result_text;
    if (result == ERROR_SUCCESS) {
      result_color = g_colors[COL_DIALOGTEXT] | FOREGROUND_GREEN & ~FOREGROUND_INTENSITY & ~FOREGROUND_RED & ~FOREGROUND_BLUE;
      result_text = far_get_msg(MSG_FILE_VER_VERIFY_OK);
    }
    else {
      if (result == TRUST_E_NOSIGNATURE)
        result = GetLastError();
      if (result == TRUST_E_NOSIGNATURE)
        result_text = far_get_msg(MSG_FILE_VER_VERIFY_BAD);
      else {
        #define RESULT_TEXT(code) if (result == code) result_text = L#code
        RESULT_TEXT(TRUST_E_PROVIDER_UNKNOWN);
        RESULT_TEXT(TRUST_E_NOSIGNATURE);
        RESULT_TEXT(TRUST_E_SUBJECT_FORM_UNKNOWN);
        RESULT_TEXT(TRUST_E_PROVIDER_UNKNOWN);
        RESULT_TEXT(TRUST_E_EXPLICIT_DISTRUST);
        RESULT_TEXT(TRUST_E_SUBJECT_NOT_TRUSTED);
        RESULT_TEXT(CRYPT_E_SECURITY_SETTINGS);
        #undef RESULT_TEXT
        if (result_text.size() == 0)
          result_text = UnicodeString::format(L"0x%x", result);
      }
      result_color = g_colors[COL_DIALOGTEXT] | FOREGROUND_RED & ~FOREGROUND_INTENSITY & ~FOREGROUND_GREEN & ~FOREGROUND_BLUE;
    }
    dlg->set_text(dlg_data->verify_result_ctrl_id, result_text);
    dlg->set_color(dlg_data->verify_result_ctrl_id, result_color);
    return TRUE;
  }
  END_ERROR_HANDLER(;,;);
  return g_far.DefDlgProc(h_dlg, msg, param1, param2);
}

void show_file_version_dialog(const UnicodeString& file_name, const VersionInfo& version_info) {
  FileVersionDialogData dlg_data;
  dlg_data.file_name = file_name;

  unsigned name_width = far_get_msg(MSG_FILE_VER_LANGUAGE).size();
  unsigned value_width = 0;
  for (unsigned i = 0; i < version_info.fixed.size(); i++) {
    if (name_width < version_info.fixed[i].name.size()) name_width = version_info.fixed[i].name.size();
    if (value_width < version_info.fixed[i].value.size()) value_width = version_info.fixed[i].value.size();
  }
  dlg_data.var_cnt = 0;
  for (unsigned i = 0; i < version_info.var.size(); i++) {
    if (dlg_data.var_cnt < version_info.var[i].strings.size()) dlg_data.var_cnt = version_info.var[i].strings.size();
    for (unsigned j = 0; j < version_info.var[i].strings.size(); j++) {
      if (name_width < version_info.var[i].strings[j].name.size()) name_width = version_info.var[i].strings[j].name.size();
      if (value_width < version_info.var[i].strings[j].value.size()) value_width = version_info.var[i].strings[j].value.size();
    }
  }
  for (unsigned i = 0; i < version_info.sig.size(); i++) {
    if (name_width < version_info.sig[i].name.size()) name_width = version_info.sig[i].name.size();
    if (value_width < version_info.sig[i].value.size()) value_width = version_info.sig[i].value.size();
  }

  ObjectArray<UnicodeString> lang_items;
  for (unsigned i = 0; i < version_info.var.size(); i++) {
    UnicodeString lang_name;
    const unsigned c_lang_name_size = 1024;
    DWORD lang_name_len = VerLanguageNameW(version_info.var[i].lang, lang_name.buf(c_lang_name_size), c_lang_name_size);
    lang_name.set_size(lang_name_len);
    if (lang_name_len)
      lang_items += lang_name;
    else
      lang_items += UnicodeString::format(L"0x%Hx%Hx", version_info.var[i].lang, version_info.var[i].code_page);
    if (value_width < lang_items.last().size()) value_width = lang_items.last().size();
  }

  name_width += 1;
  unsigned max_value_len = value_width;
  value_width += 1;
  unsigned dlg_width = name_width + value_width;
  unsigned max_width = get_msg_width();
  if (dlg_width > max_width) {
    dlg_width = max_width;
    value_width = dlg_width - name_width;
  }

  FarDialog dlg(far_get_msg(MSG_FILE_VER_TITLE), dlg_width);
  dlg.add_dlg_data(&dlg_data);
  dlg.add_dlg_data(const_cast<VersionInfo*>(&version_info));

  for (unsigned i = 0; i < version_info.fixed.size(); i++) {
    dlg.label(version_info.fixed[i].name, name_width);
    dlg.var_edit_box(version_info.fixed[i].value, version_info.fixed[i].value.size(), value_width, DIF_READONLY | DIF_SELECTONENTRY);
    dlg.new_line();
  }

  if (version_info.var.size()) {
    dlg.separator();
    dlg.new_line();
    dlg.label(far_get_msg(MSG_FILE_VER_LANGUAGE), name_width);
    if (version_info.var.size() == 1)
      dlg_data.lang_cp_ctrl_id = dlg.var_edit_box(lang_items[0], lang_items[0].size(), value_width, DIF_READONLY | DIF_SELECTONENTRY);
    else
      dlg_data.lang_cp_ctrl_id = dlg.combo_box(lang_items, 0, 10, value_width, DIF_DROPDOWNLIST | DIF_SELECTONENTRY);
    dlg.new_line();

    for (unsigned i = 0; i < dlg_data.var_cnt; i++) {
      dlg.label(UnicodeString(), name_width);
      dlg.var_edit_box(UnicodeString(), max_value_len, value_width, DIF_READONLY | DIF_SELECTONENTRY);
      dlg.new_line();
    }
  }
  else
    dlg_data.lang_cp_ctrl_id = -1;

  if (version_info.sig.size()) {
    if (version_info.fixed.size()) {
      dlg.separator(far_get_msg(MSG_FILE_VER_SIGNATURE));
      dlg.new_line();
    }
    for (unsigned i = 0; i < version_info.sig.size(); i++) {
      dlg.label(version_info.sig[i].name, name_width);
      dlg.var_edit_box(version_info.sig[i].value, version_info.sig[i].value.size(), value_width, DIF_READONLY | DIF_SELECTONENTRY);
      dlg.new_line();
    }
    dlg_data.verify_ctrl_id = dlg.button(far_get_msg(MSG_FILE_VER_VERIFY_SIGNATURE), DIF_BTNNOCLOSE);
    dlg.spacer(1);
    dlg_data.verify_result_ctrl_id = dlg.label(UnicodeString(), 30);
    dlg.new_line();
  }
  else
    dlg_data.verify_ctrl_id = -1;

  dlg.show(file_version_dialog_proc);
}

void plugin_show_file_version(const UnicodeString& file_name) {
  VersionInfo version_info = get_version_info(file_name);
  if (version_info.fixed.size() || version_info.sig.size())
    show_file_version_dialog(file_name, version_info);
  else
    far_message(far_get_msg(MSG_FILE_VER_TITLE) + L"\n" + word_wrap(far_get_msg(MSG_FILE_VER_NO_INFO), get_msg_width()) + L"\n" + far_get_msg(MSG_BUTTON_OK), 1);
}
