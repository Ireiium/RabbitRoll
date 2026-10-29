#include "componentmanager.h"


ComponentManager* ComponentManager::_instance = nullptr;

ComponentManager* ComponentManager::instance() {
    if (!_instance) {
        _instance = new ComponentManager(nullptr);
    }
    return _instance;
}
ComponentManager::ComponentManager(QObject *parent)
	: QObject(parent)
{}

ComponentManager::~ComponentManager()
{
    qDeleteAll(_components);
    _components.clear();
}

