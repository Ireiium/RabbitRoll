#pragma once

#include <QObject>
#include "componentbase.h"
#include "memberinfocomponent.h"
#include "schedulecomponent.h"
#include "editordutycomponent.h"


class ComponentManager  : public QObject
{
	Q_OBJECT

public:
    static ComponentManager* instance();

    template<typename T>
    void registerComponent(T* component) {
        if (!component) return;
        QString type = QString::fromStdString(typeid(T).name());
        _components.insert(type, component);
    }
    template<typename T>
    T* getComponent() {
        QString type = QString::fromStdString(typeid(T).name());
        return qobject_cast<T*>(_components.value(type));
    }

    ~ComponentManager();

private:
    ComponentManager(QObject* parent = nullptr);
    static ComponentManager* _instance;
    QHash<QString, ComponentBase*> _components;
};

