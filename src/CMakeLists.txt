set(kdevllm_SRCS
  kdevllm.cpp
)

ecm_qt_declare_logging_category(kdevllm_SRCS
  HEADER debug.h
  IDENTIFIER PLUGIN_KDEVLLM
  CATEGORY_NAME "kdevelop.plugins.kdevllm"
  DESCRIPTION "KDevelop plugin: kdevllm"
  EXPORT KDEVLLM
)

kdevplatform_add_plugin(kdevllm
  SOURCES ${kdevllm_SRCS}
)

target_link_libraries(kdevllm
  KDev::Interfaces
)
