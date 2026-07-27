if(NOT DEFINED INPUT_HTML OR NOT DEFINED INPUT_CSS OR NOT DEFINED INPUT_JS OR NOT DEFINED OUTPUT_HEADER)
    message(FATAL_ERROR "GenerateWebUiAssets.cmake requires INPUT_HTML, INPUT_CSS, INPUT_JS and OUTPUT_HEADER")
endif()

file(READ "${INPUT_HTML}" UI_HTML_RAW)
file(READ "${INPUT_CSS}" UI_CSS_RAW)
file(READ "${INPUT_JS}" UI_JS_RAW)

string(REPLACE "\\" "\\\\" UI_HTML_ESC "${UI_HTML_RAW}")
string(REPLACE "\"" "\\\"" UI_HTML_ESC "${UI_HTML_ESC}")
string(REPLACE "\n" "\\n" UI_HTML_ESC "${UI_HTML_ESC}")
string(REPLACE "??" "?\\?" UI_HTML_ESC "${UI_HTML_ESC}")

string(REPLACE "\\" "\\\\" UI_CSS_ESC "${UI_CSS_RAW}")
string(REPLACE "\"" "\\\"" UI_CSS_ESC "${UI_CSS_ESC}")
string(REPLACE "\n" "\\n" UI_CSS_ESC "${UI_CSS_ESC}")
string(REPLACE "??" "?\\?" UI_CSS_ESC "${UI_CSS_ESC}")

string(REPLACE "\\" "\\\\" UI_JS_ESC "${UI_JS_RAW}")
string(REPLACE "\"" "\\\"" UI_JS_ESC "${UI_JS_ESC}")
string(REPLACE "\n" "\\n" UI_JS_ESC "${UI_JS_ESC}")
string(REPLACE "??" "?\\?" UI_JS_ESC "${UI_JS_ESC}")

get_filename_component(OUTPUT_DIR "${OUTPUT_HEADER}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

file(WRITE "${OUTPUT_HEADER}" "// Generated file. Do not edit manually.\n")
file(APPEND "${OUTPUT_HEADER}" "#pragma once\n\n")
file(APPEND "${OUTPUT_HEADER}" "namespace camflow::webui {\n")
file(APPEND "${OUTPUT_HEADER}" "inline constexpr const char* kUiIndexHtml = \"${UI_HTML_ESC}\";\n")
file(APPEND "${OUTPUT_HEADER}" "inline constexpr const char* kUiAppCss = \"${UI_CSS_ESC}\";\n")
file(APPEND "${OUTPUT_HEADER}" "inline constexpr const char* kUiAppJs = \"${UI_JS_ESC}\";\n")
file(APPEND "${OUTPUT_HEADER}" "} // namespace camflow::webui\n")
