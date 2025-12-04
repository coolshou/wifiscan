#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStringListModel>
#include <QIcon>
#include <QDirIterator>

#include "src/wifiscanner.h"
#ifdef IS_DESKTOP_LINUX
#include "src/beaconmodel.h"
#endif
#include "src/beaconfilterproxymodel.h"
#include "src/interfacemodel.h"
#include "src/imageprovider.h"

#if defined(Q_OS_ANDROID)
// Android: JNI includes
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QJniObject>
#endif
#endif

void listQrcFiles()
{
    // 使用 ":" 作為根目錄，表示遍歷整個資源系統
    // QDirIterator::Subdirectories 確保遍歷所有子目錄
    QDirIterator it(":", QDirIterator::Subdirectories);

    qDebug() << "--- 列出 QRC 資原始檔 ---";
    while (it.hasNext()) {
        // next() 返回下一個資源的完整路徑（例如：":/prefix/path/file.png"）
        qDebug() << it.next();
    }
    qDebug() << "--- 列表結束 ---";
}

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
    BeaconModel *model = new BeaconModel(&app);
    QObject::connect(scanner, &WifiScanner::scanFinished, model, &BeaconModel::setBeacons);
#endif
    BeaconFilterProxyModel *filterProxy = new BeaconFilterProxyModel(&app);
    filterProxy->setSourceModel(model);

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
    engine.rootContext()->setContextProperty("beaconFilterModel", filterProxy);

    // qmlRegisterType<InterfaceModel>("Wireless", 1, 0, "InterfaceModel");
    engine.rootContext()->setContextProperty("interfaceModel", interfaceModel);
    engine.rootContext()->setContextProperty("wifiScanner", scanner);
    ImageProvider provider;
    engine.rootContext()->setContextProperty("imageProvider", &provider);

    // engine.addImportPath("qrc:///image/");
    // since Qt6.5
    // engine.loadFromModule("wifiscanner", "Main");
    // before qt6.4
    engine.load(QUrl(QStringLiteral("qrc:/wifiscanner/src/Main.qml")));
    listQrcFiles();// for debug
    return app.exec();
}
