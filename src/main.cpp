#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStringListModel>
#include <QIcon>

#include "src/wifiscanner.h"
#ifdef IS_DESKTOP_LINUX
#include "src/beaconmodel.h"
#endif
#include "src/interfacemodel.h"
#include "src/imageprovider.h"

#if defined(Q_OS_ANDROID)
// Android: JNI includes
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QJniObject>
#endif
#endif


int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName("wifiscan");
    app.setWindowIcon(QIcon(":/image/wifiscan_png"));
    // IMPORTANT: Change "wlan0" to your actual wireless interface name (e.g., wlp3s0)
    QString interfaceName = "wlan0";
    QStringListModel *interfaceModel = new QStringListModel;
    // InterfaceModel *interfaceModel = new InterfaceModel;

    WifiScanner *scanner = new WifiScanner();
    // Get interfaces and set model
    QStringList interfaces = scanner->getWirelessInterfaces();
    interfaceModel->setStringList(interfaces);
#if defined(Q_OS_ANDROID)
    QJniObject::callStaticMethod<void>("java/lang/System",
                                       "loadLibrary",
                                       "(Ljava/lang/String;)V",
                                       QJniObject::fromString("WifiScannerProject").object());
#endif
    // Connect the success signal to print results
#ifdef IS_DESKTOP_LINUX
    BeaconModel *model = new BeaconModel;
    QObject::connect(scanner, &WifiScanner::scanFinished, model, &BeaconModel::setBeacons);
#endif
    // Connect the error signal to report issues
    // QObject::connect(scanner, &WifiScanner::error,
    //                  [&app](const QString &message) {
    //                      qCritical() << "\n!!! ERROR:" << message << "!!!";
    //                      app.quit();
    //                  });

    // Start the scan (runs in a separate thread)
    scanner->startScan(interfaceName);

    QQmlApplicationEngine engine;
#ifdef IS_ANDROID
    // 1. Register QmlBeaconDetail for use in ListView models
    qmlRegisterType<QmlBeaconDetail>("tw.idv.coolshou", 1, 0, "QmlBeaconDetail");
#endif
#ifdef IS_DESKTOP_LINUX
    qmlRegisterType<BeaconModel>("Wireless", 1, 0, "BeaconModel");
    engine.rootContext()->setContextProperty("beaconModel", model);
#endif
    // qmlRegisterType<InterfaceModel>("Wireless", 1, 0, "InterfaceModel");
    engine.rootContext()->setContextProperty("interfaceModel", interfaceModel);
    engine.rootContext()->setContextProperty("wifiScanner", scanner);
    ImageProvider provider;
    engine.rootContext()->setContextProperty("imageProvider", &provider);

    // engine.addImportPath("qrc:///image/");
    engine.loadFromModule("wifiscanner", "Main");

    return app.exec();
}
