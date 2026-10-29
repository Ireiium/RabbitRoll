#pragma once

#include <QObject>

class ComponentBase : public QObject
{
    Q_OBJECT
public:
    explicit ComponentBase(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ComponentBase() = default;
};

