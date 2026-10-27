# Version the glue and binary together after linking, including incremental builds.
file(SHA256 "${WEB_DIR}/fluent_qt_gallery.js" _script_hash)
file(SHA256 "${WEB_DIR}/fluent_qt_gallery.wasm" _binary_hash)
string(SHA256 _runtime_hash "${_script_hash}${_binary_hash}")
string(SUBSTRING "${_runtime_hash}" 0 16 _runtime_hash)
file(READ "${TEMPLATE}" _page)
string(REPLACE "__FLUENT_QT_GALLERY_RUNTIME_VERSION__" "${_runtime_hash}" _page "${_page}")
file(WRITE "${WEB_DIR}/index.html" "${_page}")
