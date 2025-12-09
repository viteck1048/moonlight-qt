#pragma once

#include <QString>
#include <QVector>
#include <QStringList>

#include "SDL_compat.h"
#include "streaming/input/keyboardmapping.h"

class QXmlStreamReader;

// Intermediate structures (string tokens)
struct UserKeyKSh
{
    QString scancodeToken;
    QStringList modifierTokens;
};

struct UserKeyComboInterm
{
    UserKeyKSh input;
    QVector<UserKeyKSh> outputs;
    QString description;
};

// Working structures (SDL values)
struct UserKeyKShHx
{
    SDL_Scancode scancode = SDL_SCANCODE_UNKNOWN;
    SDL_Keymod modifiers = KMOD_NONE;
};

struct UserKeyComboHx
{
    UserKeyKShHx input;
    QVector<UserKeyKShHx> outputs;
    QString description;
};

class UserKeyComboManager final
{
public:
    static UserKeyComboManager& instance();

    bool initialize();
    bool reload();

    const QVector<UserKeyComboHx>& runtimeCombos() const;
    const QVector<UserKeyComboInterm>& configCombos() const { return m_UserKeyCombosInterm; }
    void setConfigCombos(QVector<UserKeyComboInterm> combos);
    const QString& keymapPath() const { return m_KeymapPath; }

    bool saveUserKeyCombos() const;

    UserKeyKShHx convertToRuntime(const UserKeyKSh& config, bool out_fl) const;
    UserKeyKSh convertToConfig(const SDL_KeyboardEvent* event) const;

private:
    UserKeyComboManager();

    bool ensureKeymapPath();
    bool loadUserKeyCombos();
    bool parseUserKeyComboElement(QXmlStreamReader& keymapXml, UserKeyComboInterm& combo) const;
    UserKeyKShHx buildRuntimeBinding(const UserKeyKSh& config, bool outBinding) const;

    QString m_KeymapPath;
    QVector<UserKeyComboInterm> m_UserKeyCombosInterm;
    mutable QVector<UserKeyComboHx> m_UserKeyCombosRuntime;
};
