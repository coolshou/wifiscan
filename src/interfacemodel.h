#ifndef INTERFACEMODEL_H
#define INTERFACEMODEL_H


#pragma once
#include <QStringListModel>

class InterfaceModel : public QStringListModel {
    Q_OBJECT
public:
    explicit InterfaceModel(QObject *parent = nullptr) : QStringListModel(parent) {}

    Q_INVOKABLE void updateInterfaces(const QStringList &list) {
        beginResetModel();
        setStringList(list);
        endResetModel();
    }
};

#endif // INTERFACEMODEL_H
