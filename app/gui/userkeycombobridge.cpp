#include "userkeycombobridge.h"
#include "streaming/input/userkeycombos.h"
#include <QGuiApplication>
#include <QKeyEvent>
#include <QWindow>
#include <QtGlobal>
#include <SDL.h>
#include "streaming/input/keyboardmapping.h"

namespace {

QVariantMap keySpecToVariant(const UserKeyKSh& keySpec)
{
    QVariantMap map;
    map.insert(QStringLiteral("scancode"), keySpec.scancodeToken);
    map.insert(QStringLiteral("modifiers"), keySpec.modifierTokens);
    return map;
}

QStringList variantToStringList(const QVariant& value)
{
    if (value.canConvert<QStringList>()) {
        return value.toStringList();
    }

    QStringList list;
    const QVariantList variantList = value.toList();
    list.reserve(variantList.size());
    for (const QVariant& entry : variantList) {
        list.append(entry.toString());
    }
    return list;
}

UserKeyKSh variantToKeySpec(const QVariantMap& map)
{
    UserKeyKSh keySpec;
    keySpec.scancodeToken = map.value(QStringLiteral("scancode")).toString();
    keySpec.modifierTokens = variantToStringList(map.value(QStringLiteral("modifiers")));
    return keySpec;
}

UserKeyComboInterm variantToCombo(const QVariantMap& map)
{
    UserKeyComboInterm combo;
    combo.input = variantToKeySpec(map.value(QStringLiteral("input")).toMap());

    const QVariantList outputsVar = map.value(QStringLiteral("outputs")).toList();
    for (const QVariant& outputVariant : outputsVar) {
        combo.outputs.append(variantToKeySpec(outputVariant.toMap()));
    }

    combo.description = map.value(QStringLiteral("description")).toString();
    return combo;
}

}

UserKeyComboBridge::UserKeyComboBridge(QObject* parent)
    : QObject(parent)
{
}

QVariantList UserKeyComboBridge::loadCombos() const
{
    QVariantList combos;
    const auto& configCombos = UserKeyComboManager::instance().configCombos();

    combos.reserve(configCombos.size());
    for (const UserKeyComboInterm& combo : configCombos) {
        QVariantMap comboMap;
        comboMap.insert(QStringLiteral("input"), keySpecToVariant(combo.input));

        QVariantList outputs;
        outputs.reserve(combo.outputs.size());
        for (const UserKeyKSh& outputSpec : combo.outputs) {
            outputs.append(keySpecToVariant(outputSpec));
        }
        comboMap.insert(QStringLiteral("outputs"), outputs);
        comboMap.insert(QStringLiteral("description"), combo.description);

        combos.append(comboMap);
    }

    return combos;
}

bool UserKeyComboBridge::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        
        // Convert Qt key event to SDL keyboard event using centralized scancode mapper
        SDL_KeyboardEvent sdlEvent{};
        sdlEvent.type = SDL_KEYDOWN;
        sdlEvent.timestamp = SDL_GetTicks();
        sdlEvent.windowID = 0;
        sdlEvent.state = SDL_PRESSED;
        sdlEvent.repeat = keyEvent->isAutoRepeat() ? 1 : 0;

        LikeSdlScancodeMapper& mapper = LikeSdlScancodeMapper::instance();
        mapper.initialize();
        SDL_Scancode scancode = mapper.nativeToScancode(keyEvent->nativeScanCode());
        sdlEvent.keysym.scancode = scancode;
        sdlEvent.keysym.sym = static_cast<SDL_Keycode>(scancode);
        
        // Convert Qt modifiers to SDL modifiers
        Qt::KeyboardModifiers qtModifiers = keyEvent->modifiers();
        sdlEvent.keysym.mod = KMOD_NONE;
        if (qtModifiers & Qt::ShiftModifier) sdlEvent.keysym.mod = static_cast<SDL_Keymod>(sdlEvent.keysym.mod | KMOD_SHIFT);
        if (qtModifiers & Qt::ControlModifier) sdlEvent.keysym.mod = static_cast<SDL_Keymod>(sdlEvent.keysym.mod | KMOD_CTRL);
        if (qtModifiers & Qt::AltModifier) sdlEvent.keysym.mod = static_cast<SDL_Keymod>(sdlEvent.keysym.mod | KMOD_ALT);
        if (qtModifiers & Qt::MetaModifier) sdlEvent.keysym.mod = static_cast<SDL_Keymod>(sdlEvent.keysym.mod | KMOD_GUI);
        
        // Use UserKeyComboManager to convert the SDL event to config
        UserKeyKSh config = UserKeyComboManager::instance().convertToConfig(&sdlEvent);
        
        // Create a QVariantMap to return the result
        QVariantMap resultMap;
        resultMap["scancode"] = config.scancodeToken;
        resultMap["modifiers"] = config.modifierTokens;
        
        QVariantList resultList;
        resultList.append(resultMap);
        
        // If we're in key capture mode, build the token string
        if (m_capturingKey) {
            QStringList tokens;
            for (const QString& mod : config.modifierTokens) {
                if (!mod.isEmpty()) {
                    tokens.append(mod);
                }
            }
            if (!config.scancodeToken.isEmpty()) {
                tokens.append(config.scancodeToken);
            }
            m_capturedKeyCombo = tokens.join("+");
            m_capturingKey = false;
        }
        
        // Emit a signal with the captured key data
        emit keyCaptured(resultList);
        
        // Remove the event filter after capturing the key
        watched->removeEventFilter(this);
        
        return true; // Event handled
    }
    
    return QObject::eventFilter(watched, event);
}

bool UserKeyComboBridge::saveCombos(const QVariantList& combos) const
{
    QVector<UserKeyComboInterm> comboList;
    comboList.reserve(combos.size());

    for (const QVariant& comboVariant : combos) {
        const QVariantMap comboMap = comboVariant.toMap();
        comboList.append(variantToCombo(comboMap));
    }

    UserKeyComboManager::instance().setConfigCombos(comboList);
    return UserKeyComboManager::instance().saveUserKeyCombos();
}

bool UserKeyComboBridge::reloadCombos() const
{
    return UserKeyComboManager::instance().reload();
}

QVariantMap UserKeyComboBridge::emptyKeySpec() const
{
    UserKeyKSh keySpec;
    auto map = keySpecToVariant(keySpec);
    map.insert(QStringLiteral("autoDetect"), true);
    return map;
}

QVariantMap UserKeyComboBridge::emptyCombo() const
{
    QVariantMap comboMap;
    comboMap.insert(QStringLiteral("input"), emptyKeySpec());

    QVariantList outputs;
    outputs.append(emptyKeySpec());
    comboMap.insert(QStringLiteral("outputs"), outputs);

    comboMap.insert(QStringLiteral("description"), QString());
    comboMap.insert(QStringLiteral("autoDetectMode"), true);
    return comboMap;
}

SDL_Keymod UserKeyComboBridge::nativeModifiersToSDL(int nativeModifiers, SDL_Scancode scancode)
{
    int sdlMod = 0;
    sdlMod |= (nativeModifiers & 0x02000000) && !(scancode == SDL_SCANCODE_LSHIFT || scancode == SDL_SCANCODE_RSHIFT) ? KMOD_SHIFT : 0;
    sdlMod |= (nativeModifiers & 0x04000000) && !(scancode == SDL_SCANCODE_LCTRL || scancode == SDL_SCANCODE_RCTRL) ? KMOD_CTRL : 0;
    sdlMod |= (nativeModifiers & 0x08000000) && !(scancode == SDL_SCANCODE_LALT || scancode == SDL_SCANCODE_RALT) ? KMOD_ALT : 0;
    sdlMod |= (nativeModifiers & 0x10000000) && !(scancode == SDL_SCANCODE_LGUI || scancode == SDL_SCANCODE_RGUI) ? KMOD_GUI : 0;

    return (SDL_Keymod)sdlMod;
}

QString UserKeyComboBridge::keyEventToTokens(int nativeScancode, int nativeModifiers)
{
    LikeSdlScancodeMapper& mapper = LikeSdlScancodeMapper::instance();
    mapper.initialize();
    
    // Convert native scancode to SDL scancode
    SDL_Scancode scancode = mapper.nativeToScancode(nativeScancode);
    
    SDL_Keymod sdlMod = nativeModifiersToSDL(nativeModifiers, scancode);
    
    // Get modifier tokens
    QStringList modTokens = mapper.nativeToModifiers(sdlMod);
    
    // Get scancode token
    const char* scancodeToken = mapper.scancodeToToken(scancode);
    
    // Build the combined token string
    QStringList tokens;
    for (const QString& mod : modTokens) {
        if (!mod.isEmpty()) {
            tokens.append(mod);
        }
    }
    if (scancodeToken && scancodeToken[0] != '\0') {
        tokens.append(QString::fromLatin1(scancodeToken));
    }
    
    return tokens.join("+");
}

QString UserKeyComboBridge::keymapPath() const
{
    return UserKeyComboManager::instance().keymapPath();
}