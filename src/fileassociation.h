#pragma once

#include <QString>

// Registers the portable executable for the current user, preserving other defaults.
bool registerRrollFileAssociation(const QString& executablePath, QString* error = nullptr);
