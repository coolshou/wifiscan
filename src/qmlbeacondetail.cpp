#include "qmlbeacondetail.h"

// --- QmlBeaconDetail Implementation ---
QmlBeaconDetail::QmlBeaconDetail(const BeaconDetail& detail, QObject* parent)
    : QObject(parent), m_detail(detail)
{}
