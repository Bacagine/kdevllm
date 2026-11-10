set(kdevllm_SRCS
    kdevllmplugin.cpp
    llmclient.cpp
    llmchatwidget.cpp
    llmsettingsdialog.cpp
    llmcredentialstore.cpp
)

set(kdevllm_UI
    llmchatwidget.ui
    llmsettingsdialog.ui
)

if(KDEVLLM_QT_VER EQUAL 4)
    # --- KDE4 / Qt4 build ---
    kde4_add_ui_files(kdevllm_SRCS ${kdevllm_UI})
    kde4_add_plugin(kdevllm ${kdevllm_SRCS})
    target_link_libraries(kdevllm
        ${KDE4_KDECORE_LIBS}
        ${KDE4_KDEUI_LIBS}
        ${KDE4_KPARTS_LIBS}
        ${QT_QTNETWORK_LIBRARY}
        ktexteditor
        kdevplatforminterfaces
        kdevplatformutil
        kdevplatformlanguage
    )
    install(TARGETS kdevllm DESTINATION ${PLUGIN_INSTALL_DIR})
    install(FILES kdevllm.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kdevelop)
else()
    # --- KDev5/Qt5 or KDev6/Qt6 build ---
    ecm_qt_declare_logging_category(kdevllm_SRCS
        HEADER debug.h
        IDENTIFIER PLUGIN_KDEVLLM
        CATEGORY_NAME "kdevelop.plugins.kdevllm"
        DESCRIPTION "KDevelop plugin: LLM integration"
        EXPORT KDEVLLM
    )

    if(KDEVLLM_QT_VER EQUAL 6)
        qt_wrap_ui(kdevllm_SRCS ${kdevllm_UI})
    else()
        qt5_wrap_ui(kdevllm_SRCS ${kdevllm_UI})
    endif()

    kdevplatform_add_plugin(kdevllm SOURCES ${kdevllm_SRCS} kdevllm.json)

    target_link_libraries(kdevllm
        KF${KDEVLLM_KF_VER}::I18n
        KF${KDEVLLM_KF_VER}::Parts
        KF${KDEVLLM_KF_VER}::TextEditor
        KF${KDEVLLM_KF_VER}::KIOWidgets
        KF${KDEVLLM_KF_VER}::Wallet
        Qt${KDEVLLM_QT_VER}::Network
        KDev::Interfaces
        KDev::Util
        KDev::Language
    )

    install(TARGETS kdevllm
        DESTINATION ${KDE_INSTALL_QTPLUGINDIR}/kdevplatform/${KDEVLLM_ABI_VER}
    )

    ecm_qt_install_logging_categories(
        EXPORT KDEVLLM
        FILE kdevllm.categories
        DESTINATION "${KDE_INSTALL_LOGGINGCATEGORIESDIR}"
    )
endif()
