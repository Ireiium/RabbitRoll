#pragma once

#include <QObject>
#include <QVariant>
#include "RTypies.h"

class ModelInterface
{
public:
    virtual ~ModelInterface() = default;
    //获取
    virtual QVariant get(const RTypies::Model& key) const = 0;
    virtual QVariant getByIndex(const RTypies::Model& key, int index) const = 0;
    //操作
    virtual QVariant exec(RTypies::Model action, const QVariant& val = QVariant()) = 0;
};

