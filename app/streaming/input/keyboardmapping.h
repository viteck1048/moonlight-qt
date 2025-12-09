#pragma once

#include <SDL.h>

#include <QHash>
#include <QString>
#include <QVector>

#include <cstdint>

class LikeSdlScancodeMapper
{
public:
    static LikeSdlScancodeMapper& instance();

    bool initialize();

    const char* scancodeToToken(SDL_Scancode scancode) const;
    SDL_Scancode tokenToScancode(const QString& token) const;
    SDL_Scancode nativeToScancode(uint32_t nativeScancode) const;
    
    QStringList nativeToModifiers(SDL_Keymod nativeMod) const;

private:
    LikeSdlScancodeMapper();

    void buildScancodeTables();
    QString normalizeToken(const QString& token) const;

private:
    bool m_initialized = false;
    QVector<const char*> m_scancodeTokens; // index by SDL_Scancode value
    QHash<QString, SDL_Scancode> m_tokenToScancode;
};
