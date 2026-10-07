#ifndef FLUENTWINDOWBACKDROPXCB_P_H
#define FLUENTWINDOWBACKDROPXCB_P_H

#include <QLibrary>
#include <QGuiApplication>
#include <QtGlobal>

#if defined(Q_OS_LINUX) && QT_VERSION >= QT_VERSION_CHECK(6, 2, 0)
#include <QtGui/qguiapplication_platform.h>
#endif

#include <cstddef>
#include <cstdlib>
#include <memory>

#include "WindowBackdropX11Protocol_p.h"

namespace compatibility::detail::xcb {

// Core XCB wire layouts contain fixed-width fields on both ILP32 and LP64.
// Loading the optional runtime ABI keeps X11 headers/libraries out of the
// base FluentQt build, including Wayland-only Qt installations.
using Connection = struct xcb_connection_t;
using Setup = struct xcb_setup_t;
using Error = struct xcb_generic_error_t;

// Qt 6 exposes its own connection publicly; older Qt uses the screen round trip.
// zh_CN: Qt 6 公开其 XCB 连接；旧版 Qt 通过 screen 往返完成同步。
inline Connection* qtConnection()
{
#if defined(Q_OS_LINUX) && QT_VERSION >= QT_VERSION_CHECK(6, 2, 0) && QT_CONFIG(xcb)
    if (qGuiApp) {
        if (auto* native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>())
            return reinterpret_cast<Connection*>(native->connection());
    }
#endif
    return nullptr;
}

struct Cookie {
    unsigned int sequence;
};

template <typename T> struct Iterator {
    T* data;
    int remaining;
    int index;
};

struct Visual {
    quint32 id;
    quint8 visualClass;
    quint8 bitsPerRgb;
    quint16 colormapEntries;
    quint32 redMask;
    quint32 greenMask;
    quint32 blueMask;
    quint8 padding[4];
};

struct Depth {
    quint8 depth;
    quint8 padding0;
    quint16 visualsLength;
    quint8 padding1[4];
};

struct Screen {
    quint32 root;
    quint32 defaultColormap;
    quint32 whitePixel;
    quint32 blackPixel;
    quint32 currentInputMasks;
    quint16 width;
    quint16 height;
    quint16 widthMillimeters;
    quint16 heightMillimeters;
    quint16 minimumMaps;
    quint16 maximumMaps;
    quint32 rootVisual;
    quint8 backingStores;
    quint8 saveUnders;
    quint8 rootDepth;
    quint8 depthsLength;
};

struct AttributesReply {
    quint8 responseType;
    quint8 backingStore;
    quint16 sequence;
    quint32 length;
    quint32 visual;
    quint16 windowClass;
    quint8 bitGravity;
    quint8 windowGravity;
    quint32 backingPlanes;
    quint32 backingPixel;
    quint8 saveUnder;
    quint8 mapInstalled;
    quint8 mapState;
    quint8 overrideRedirect;
    quint32 colormap;
    quint32 allEventMasks;
    quint32 yourEventMask;
    quint16 doNotPropagateMask;
    quint8 padding[2];
};

struct GeometryReply {
    quint8 responseType;
    quint8 depth;
    quint16 sequence;
    quint32 length;
    quint32 root;
    qint16 x;
    qint16 y;
    quint16 width;
    quint16 height;
    quint16 borderWidth;
    quint8 padding[2];
};

struct IdReply {
    quint8 responseType;
    quint8 padding0;
    quint16 sequence;
    quint32 length;
    quint32 id;
    quint8 padding1[20];
};

struct ListPropertiesReply {
    quint8 responseType;
    quint8 padding0;
    quint16 sequence;
    quint32 length;
    quint16 atomsLength;
    quint8 padding1[22];
};

struct PropertyReply {
    quint8 responseType;
    quint8 format;
    quint16 sequence;
    quint32 length;
    quint32 type;
    quint32 bytesAfter;
    quint32 valueLength;
    quint8 padding[12];
};

static_assert(sizeof(Cookie) == 4 && sizeof(Visual) == 24 && sizeof(Depth) == 8 &&
                  sizeof(Screen) == 40 && offsetof(Screen, rootVisual) == 32,
              "The dynamic XCB adapter must retain the core setup ABI");
static_assert(sizeof(AttributesReply) == 44 && offsetof(AttributesReply, visual) == 8 &&
                  sizeof(GeometryReply) == 24 && offsetof(GeometryReply, root) == 8 &&
                  sizeof(IdReply) == 32 && sizeof(ListPropertiesReply) == 32 &&
                  sizeof(PropertyReply) == 32,
              "The dynamic XCB adapter must retain the core reply ABI");

struct FreeReply {
    void operator()(void* value) const { std::free(value); }
};

template <typename T> using Reply = std::unique_ptr<T, FreeReply>;

struct Api {
    using Connect = Connection* (*)(const char*, int*);
    using Disconnect = void (*)(Connection*);
    using ConnectionHasError = int (*)(Connection*);
    using GetSetup = const Setup* (*)(Connection*);
    using RootsIterator = Iterator<Screen> (*)(const Setup*);
    using ScreenNext = void (*)(Iterator<Screen>*);
    using DepthsIterator = Iterator<Depth> (*)(const Screen*);
    using DepthNext = void (*)(Iterator<Depth>*);
    using VisualsIterator = Iterator<Visual> (*)(const Depth*);
    using VisualNext = void (*)(Iterator<Visual>*);
    using WindowRequest = Cookie (*)(Connection*, quint32);
    using FocusRequest = Cookie (*)(Connection*);
    template <typename T> using ReplyFunction = T* (*)(Connection*, Cookie, Error**);
    using InternAtom = Cookie (*)(Connection*, quint8, quint16, const char*);
    using ListPropertyAtoms = quint32* (*)(const ListPropertiesReply*);
    using GetProperty = Cookie (*)(Connection*, quint8, quint32, quint32, quint32, quint32,
                                   quint32);
    using PropertyValue = void* (*)(const PropertyReply*);
    using PropertyValueLength = int (*)(const PropertyReply*);
    using ChangeProperty = Cookie (*)(Connection*, quint8, quint32, quint32, quint32, quint8,
                                      quint32, const void*);
    using DeleteProperty = Cookie (*)(Connection*, quint32, quint32);
    using ChangeAttributes = Cookie (*)(Connection*, quint32, quint32, const void*);
    using ConfigureWindow = Cookie (*)(Connection*, quint32, quint16, const void*);
    using SetInputFocus = Cookie (*)(Connection*, quint8, quint32, quint32);
    using RequestCheck = Error* (*)(Connection*, Cookie);

    Connect connect = nullptr;
    Disconnect disconnect = nullptr;
    ConnectionHasError connectionHasError = nullptr;
    GetSetup getSetup = nullptr;
    RootsIterator rootsIterator = nullptr;
    ScreenNext screenNext = nullptr;
    DepthsIterator depthsIterator = nullptr;
    DepthNext depthNext = nullptr;
    VisualsIterator visualsIterator = nullptr;
    VisualNext visualNext = nullptr;
    WindowRequest getWindowAttributes = nullptr;
    ReplyFunction<AttributesReply> getWindowAttributesReply = nullptr;
    WindowRequest getGeometry = nullptr;
    ReplyFunction<GeometryReply> getGeometryReply = nullptr;
    InternAtom internAtom = nullptr;
    ReplyFunction<IdReply> internAtomReply = nullptr;
    WindowRequest getSelectionOwner = nullptr;
    ReplyFunction<IdReply> getSelectionOwnerReply = nullptr;
    WindowRequest listProperties = nullptr;
    ReplyFunction<ListPropertiesReply> listPropertiesReply = nullptr;
    ListPropertyAtoms listPropertyAtoms = nullptr;
    GetProperty getProperty = nullptr;
    ReplyFunction<PropertyReply> getPropertyReply = nullptr;
    PropertyValue propertyValue = nullptr;
    PropertyValueLength propertyValueLength = nullptr;
    ChangeProperty changePropertyChecked = nullptr;
    DeleteProperty deletePropertyChecked = nullptr;
    ChangeAttributes changeAttributesChecked = nullptr;
    WindowRequest mapWindowChecked = nullptr;
    ConfigureWindow configureWindowChecked = nullptr;
    SetInputFocus setInputFocusChecked = nullptr;
    FocusRequest getInputFocus = nullptr;
    ReplyFunction<IdReply> getInputFocusReply = nullptr;
    RequestCheck requestCheck = nullptr;

    bool load()
    {
        if (m_attempted)
            return m_loaded;
        m_attempted = true;
        if (!m_library.load())
            return false;
        m_loaded = resolve(connect, "xcb_connect") && resolve(disconnect, "xcb_disconnect") &&
                   resolve(connectionHasError, "xcb_connection_has_error") &&
                   resolve(getSetup, "xcb_get_setup") &&
                   resolve(rootsIterator, "xcb_setup_roots_iterator") &&
                   resolve(screenNext, "xcb_screen_next") &&
                   resolve(depthsIterator, "xcb_screen_allowed_depths_iterator") &&
                   resolve(depthNext, "xcb_depth_next") &&
                   resolve(visualsIterator, "xcb_depth_visuals_iterator") &&
                   resolve(visualNext, "xcb_visualtype_next") &&
                   resolve(getWindowAttributes, "xcb_get_window_attributes") &&
                   resolve(getWindowAttributesReply, "xcb_get_window_attributes_reply") &&
                   resolve(getGeometry, "xcb_get_geometry") &&
                   resolve(getGeometryReply, "xcb_get_geometry_reply") &&
                   resolve(internAtom, "xcb_intern_atom") &&
                   resolve(internAtomReply, "xcb_intern_atom_reply") &&
                   resolve(getSelectionOwner, "xcb_get_selection_owner") &&
                   resolve(getSelectionOwnerReply, "xcb_get_selection_owner_reply") &&
                   resolve(listProperties, "xcb_list_properties") &&
                   resolve(listPropertiesReply, "xcb_list_properties_reply") &&
                   resolve(listPropertyAtoms, "xcb_list_properties_atoms") &&
                   resolve(getProperty, "xcb_get_property") &&
                   resolve(getPropertyReply, "xcb_get_property_reply") &&
                   resolve(propertyValue, "xcb_get_property_value") &&
                   resolve(propertyValueLength, "xcb_get_property_value_length") &&
                   resolve(changePropertyChecked, "xcb_change_property_checked") &&
                   resolve(deletePropertyChecked, "xcb_delete_property_checked") &&
                   resolve(changeAttributesChecked, "xcb_change_window_attributes_checked") &&
                   resolve(mapWindowChecked, "xcb_map_window_checked") &&
                   resolve(configureWindowChecked, "xcb_configure_window_checked") &&
                   resolve(setInputFocusChecked, "xcb_set_input_focus_checked") &&
                   resolve(getInputFocus, "xcb_get_input_focus") &&
                   resolve(getInputFocusReply, "xcb_get_input_focus_reply") &&
                   resolve(requestCheck, "xcb_request_check");
        return m_loaded;
    }

    bool healthy(Connection* connection) const
    {
        return connection && connectionHasError && connectionHasError(connection) == 0;
    }

    template <typename T>
    Reply<T> reply(Connection* connection, Cookie cookie, ReplyFunction<T> readReply) const
    {
        if (!healthy(connection) || !cookie.sequence || !readReply)
            return {};
        Error* error = nullptr;
        Reply<T> result(readReply(connection, cookie, &error));
        Reply<Error> ownedError(error);
        if (ownedError || !healthy(connection))
            result.reset();
        return result;
    }

    bool check(Connection* connection, Cookie cookie) const
    {
        if (!healthy(connection) || !cookie.sequence || !requestCheck)
            return false;
        Reply<Error> error(requestCheck(connection, cookie));
        return !error && healthy(connection);
    }

    bool roundTrip(Connection* connection) const
    {
        if (!healthy(connection) || !getInputFocus || !getInputFocusReply)
            return false;
        const Cookie cookie = getInputFocus(connection);
        if (!cookie.sequence)
            return false;
        Error* error = nullptr;
        Reply<IdReply> result(getInputFocusReply(connection, cookie, &error));
        Reply<Error> ownedError(error);
        return x11ClientRoundTripCompleted(true, result != nullptr, ownedError != nullptr,
                                           !healthy(connection));
    }

private:
    template <typename T> bool resolve(T& function, const char* name)
    {
        function = reinterpret_cast<T>(m_library.resolve(name));
        return function != nullptr;
    }

    QLibrary m_library{QStringLiteral("xcb"), 1};
    bool m_attempted = false;
    bool m_loaded = false;
};

} // namespace compatibility::detail::xcb

#endif // FLUENTWINDOWBACKDROPXCB_P_H
