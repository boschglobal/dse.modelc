# Copyright 2026 Robert Bosch GmbH
#
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)

if(DEFINED ENV{DSE_EXTERNAL_DOWNLOAD_DIR})
    set(_dse_external_download_dir "$ENV{DSE_EXTERNAL_DOWNLOAD_DIR}")
else()
    set(_dse_external_download_dir "/tmp/dse_cache")
endif()
set(DSE_EXTERNAL_DOWNLOAD_DIR "${_dse_external_download_dir}" CACHE PATH "Download cache for DSE external projects.")

# Download <url> into the cache (once) and set <out_var> to the cached archive, <out_var>_HASH to its hash.
function(dse_external_download name url username password out_var)
    get_filename_component(_file "${url}" NAME)
    string(MD5 _key "${url}")
    string(SUBSTRING "${_key}" 0 8 _key)
    set(_path "${DSE_EXTERNAL_DOWNLOAD_DIR}/${name}/${_key}-${_file}")
    if(NOT EXISTS "${_path}")
        set(_auth "")
        if(username)
            set(_auth USERPWD "${username}:${password}")
        endif()
        # Download to a unique temporary name so concurrent builds never see a partial archive.
        string(RANDOM LENGTH 8 _tmp)
        file(DOWNLOAD "${url}" "${_path}.${_tmp}.part" ${_auth} STATUS _status)
        list(GET _status 0 _code)
        if(NOT _code EQUAL 0)
            file(REMOVE "${_path}.${_tmp}.part")
            message(FATAL_ERROR "Download of ${url} failed: ${_status}")
        endif()
        file(RENAME "${_path}.${_tmp}.part" "${_path}")
    endif()
    # Hash of the cached archive (not a trust check), avoids ExternalProject's "no URL_HASH" warning.
    file(SHA256 "${_path}" _sha256)
    set(${out_var} "${_path}" PARENT_SCOPE)
    set(${out_var}_HASH "SHA256=${_sha256}" PARENT_SCOPE)
endfunction()
