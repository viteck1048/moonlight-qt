#include "streaming/input/userkeycombos.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>
#include <QXmlStreamReader>
#include <utility>

#define SER_USERKEYCOMBOS "userKeyCombos"
#define SER_INMOD "inMod"
#define SER_INCODE "inCode"
#define SER_DESCRIPTION "description"
#define SER_OUTPUTS "outputs"
#define SER_MOD "mod"
#define SER_CODE "code"

UserKeyComboManager& UserKeyComboManager::instance()
{
    static UserKeyComboManager instance;
    return instance;
}

UserKeyComboManager::UserKeyComboManager()
{
}

bool UserKeyComboManager::initialize()
{
    return loadUserKeyCombos();
}

bool UserKeyComboManager::reload()
{
    return loadUserKeyCombos();
}

static QStringList splitModifierTokens(const QString& text)
{
    return text.split(QLatin1String("|"), Qt::SkipEmptyParts);
}

bool UserKeyComboManager::loadUserKeyCombos()
{
    QSettings settings;

    // First run with this storage: pick up the old keymap.xml if there is one
    if (!settings.contains(SER_USERKEYCOMBOS "/size")) {
        importLegacyKeymapXml();
        return true;
    }

    m_UserKeyCombosInterm.clear();

    int comboCount = settings.beginReadArray(SER_USERKEYCOMBOS);
    for (int i = 0; i < comboCount; i++) {
        settings.setArrayIndex(i);

        UserKeyComboInterm combo;
        combo.input.modifierTokens = splitModifierTokens(settings.value(SER_INMOD).toString());
        combo.input.scancodeToken = settings.value(SER_INCODE).toString();
        combo.description = settings.value(SER_DESCRIPTION).toString();

        int outputCount = settings.beginReadArray(SER_OUTPUTS);
        for (int j = 0; j < outputCount; j++) {
            settings.setArrayIndex(j);

            UserKeyKSh output;
            output.modifierTokens = splitModifierTokens(settings.value(SER_MOD).toString());
            output.scancodeToken = settings.value(SER_CODE).toString();
            combo.outputs.append(output);
        }
        settings.endArray();

        m_UserKeyCombosInterm.append(combo);
    }
    settings.endArray();

    return true;
}

bool UserKeyComboManager::importLegacyKeymapXml()
{
    // Older builds kept the combos in keymap.xml. Check every place it could be.
    QStringList candidates;
    candidates << QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
               << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
               << QCoreApplication::applicationDirPath();

    for (const QString& directory : candidates) {
        if (directory.isEmpty()) {
            continue;
        }

        const QString legacyPath = QDir(directory).filePath(QStringLiteral("keymap.xml"));
        if (!QFile::exists(legacyPath)) {
            continue;
        }

        if (!loadKeymapXml(legacyPath)) {
            continue;
        }

        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                    "Imported %d key combos from %s",
                    (int)m_UserKeyCombosInterm.size(),
                    legacyPath.toUtf8().constData());
        return saveUserKeyCombos();
    }

    return false;
}

bool UserKeyComboManager::loadKeymapXml(const QString& path)
{
    QFile keymapFile(path);
    if (!keymapFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Keymap config not loaded (path: %s): %s",
                    path.toUtf8().constData(),
                    keymapFile.errorString().toUtf8().constData());
        return false;
    }
    
    QVector<UserKeyComboInterm> combos;
    
    QXmlStreamReader keymapXml(&keymapFile);
    while (!keymapXml.atEnd() && !keymapXml.hasError()) {
        keymapXml.readNext();
        
        if (!keymapXml.isStartElement()) {
            continue;
        }
        
        if (keymapXml.name() == QLatin1String("userKeyCombo")) {
            UserKeyComboInterm combo;
            if (parseUserKeyComboElement(keymapXml, combo)) {
                combos.append(combo);
            }
        }
    }
    
    if (keymapXml.hasError()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Keymap XML parse error at line %lld: %s",
                    keymapXml.lineNumber(),
                    keymapXml.errorString().toUtf8().constData());
        return false;
    }
    
    m_UserKeyCombosInterm = std::move(combos);
    return true;
}

const QVector<UserKeyComboHx>& UserKeyComboManager::runtimeCombos() const
{
    // Clear the old working array
    m_UserKeyCombosRuntime.clear();
    
    // Rebuild the working array from the intermediate one
    m_UserKeyCombosRuntime.reserve(m_UserKeyCombosInterm.size());
    
    for (const UserKeyComboInterm& configCombo : m_UserKeyCombosInterm) {
        UserKeyComboHx runtimeCombo;
        runtimeCombo.input = convertToRuntime(configCombo.input, false);
        
        for (const UserKeyKSh& outputConfig : configCombo.outputs) {
            runtimeCombo.outputs.append(convertToRuntime(outputConfig, true));
        }
        
        runtimeCombo.description = configCombo.description;
        m_UserKeyCombosRuntime.append(runtimeCombo);
    }
    
    return m_UserKeyCombosRuntime;
}

void UserKeyComboManager::setConfigCombos(QVector<UserKeyComboInterm> combos)
{
    m_UserKeyCombosInterm = std::move(combos);
}

bool UserKeyComboManager::saveUserKeyCombos() const
{
    QSettings settings;

    // Drop stale entries in case the new list is shorter
    settings.remove(SER_USERKEYCOMBOS);

    settings.beginWriteArray(SER_USERKEYCOMBOS, m_UserKeyCombosInterm.size());
    for (int i = 0; i < m_UserKeyCombosInterm.size(); i++) {
        const UserKeyComboInterm& combo = m_UserKeyCombosInterm[i];
        settings.setArrayIndex(i);

        settings.setValue(SER_INMOD, combo.input.modifierTokens.join(QLatin1String("|")));
        settings.setValue(SER_INCODE, combo.input.scancodeToken);
        settings.setValue(SER_DESCRIPTION, combo.description);

        settings.beginWriteArray(SER_OUTPUTS, combo.outputs.size());
        for (int j = 0; j < combo.outputs.size(); j++) {
            settings.setArrayIndex(j);
            settings.setValue(SER_MOD, combo.outputs[j].modifierTokens.join(QLatin1String("|")));
            settings.setValue(SER_CODE, combo.outputs[j].scancodeToken);
        }
        settings.endArray();
    }
    settings.endArray();

    settings.sync();
    if (settings.status() != QSettings::NoError) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to save key combos to settings");
        return false;
    }

    return true;
}

UserKeyKShHx UserKeyComboManager::convertToRuntime(const UserKeyKSh& config, bool out_fl) const
{
    UserKeyKShHx runtime;

    LikeSdlScancodeMapper& mapper = LikeSdlScancodeMapper::instance();
    mapper.initialize();

    runtime.scancode = mapper.tokenToScancode(config.scancodeToken);
     
    // Convert modifier tokens to SDL_Keymod
    runtime.modifiers = KMOD_NONE;
    for (const QString& token : config.modifierTokens) {
        if (token == QLatin1String("KMOD_LSHIFT")) {
            runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_LSHIFT);
        }
        else if (token == QLatin1String("KMOD_RSHIFT")) {
            runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_RSHIFT);
        }
        else if (token == QLatin1String("KMOD_LCTRL")) {
            runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_LCTRL);
        }
        else if (token == QLatin1String("KMOD_RCTRL")) {
            runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_RCTRL);
        }
        else if (token == QLatin1String("KMOD_LALT")) {
            runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_LALT);
        }
        else if (token == QLatin1String("KMOD_RALT")) {
            runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_RALT);
        }
        else if (token == QLatin1String("KMOD_LGUI")) {
            runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_LGUI);
        }
        else if (token == QLatin1String("KMOD_RGUI")) {
            runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_RGUI);
        }
        else if (token == QLatin1String("KMOD_CTRL")) {
            if(out_fl)
                runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_LCTRL);
            else
                runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_CTRL);
        }
        else if (token == QLatin1String("KMOD_SHIFT")) {
            if(out_fl)
                runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_LSHIFT);
            else
                runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_SHIFT);
        }
        else if (token == QLatin1String("KMOD_ALT")) {
            if(out_fl)
                runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_LALT);
            else
                runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_ALT);
        }
        else if (token == QLatin1String("KMOD_GUI")) {
            if(out_fl)
                runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_LGUI);
            else
                runtime.modifiers = (SDL_Keymod)(runtime.modifiers | KMOD_GUI);
        }
    }
    
    return runtime;
}

UserKeyKSh UserKeyComboManager::convertToConfig(const SDL_KeyboardEvent* event) const
{
    UserKeyKSh config;
    
    if (!event) {
        return config;
    }
    
    LikeSdlScancodeMapper& mapper = LikeSdlScancodeMapper::instance();
    mapper.initialize();

    // Convert scancode to token
    SDL_Scancode scancode = event->keysym.scancode;
    const char* scancodeToken = mapper.scancodeToToken(scancode);
    if (scancodeToken && scancodeToken[0] != '\0') {
        config.scancodeToken = QString::fromLatin1(scancodeToken);
    } else {
        config.scancodeToken = QStringLiteral("SDL_SCANCODE_UNKNOWN");
    }
    
    // Convert modifiers to tokens
    SDL_Keymod modifiers = static_cast<SDL_Keymod>(event->keysym.mod);
    if (modifiers & KMOD_LSHIFT) config.modifierTokens.append("KMOD_LSHIFT");
    if (modifiers & KMOD_RSHIFT) config.modifierTokens.append("KMOD_RSHIFT");
    if (modifiers & KMOD_LCTRL) config.modifierTokens.append("KMOD_LCTRL");
    if (modifiers & KMOD_RCTRL) config.modifierTokens.append("KMOD_RCTRL");
    if (modifiers & KMOD_LALT) config.modifierTokens.append("KMOD_LALT");
    if (modifiers & KMOD_RALT) config.modifierTokens.append("KMOD_RALT");
    if (modifiers & KMOD_LGUI) config.modifierTokens.append("KMOD_LGUI");
    if (modifiers & KMOD_RGUI) config.modifierTokens.append("KMOD_RGUI");
    
    return config;
}

bool UserKeyComboManager::parseUserKeyComboElement(QXmlStreamReader& keymapXml, UserKeyComboInterm& combo) const
{
    combo = UserKeyComboInterm(); // Initialize with default values
    
    while (!keymapXml.atEnd()) {
        keymapXml.readNext();
        
        if (keymapXml.isEndElement() && keymapXml.name() == QLatin1String("userKeyCombo")) {
            return true; // Successfully parsed the element
        }
        
        if (!keymapXml.isStartElement()) {
            continue;
        }
        
        if (keymapXml.name() == QLatin1String("in")) {
            // Parse input binding
            UserKeyKSh input;
            while (!keymapXml.atEnd()) {
                keymapXml.readNext();
                
                if (keymapXml.isEndElement() && keymapXml.name() == QLatin1String("in")) {
                    break;
                }
                
                if (!keymapXml.isStartElement()) {
                    continue;
                }
                
                if (keymapXml.name() == QLatin1String("mod")) {
                    QString modifierText = keymapXml.readElementText();
                    if (!modifierText.isEmpty()) {
                        input.modifierTokens = modifierText.split(QLatin1String("|"), Qt::SkipEmptyParts);
                    }
                }
                else if (keymapXml.name() == QLatin1String("code")) {
                    input.scancodeToken = keymapXml.readElementText();
                }
            }
            combo.input = input;
        }
        else if (keymapXml.name() == QLatin1String("description")) {
            combo.description = keymapXml.readElementText();
        }
        else if (keymapXml.name() == QLatin1String("outArr")) {
            // Parse output bindings array
            while (!keymapXml.atEnd()) {
                keymapXml.readNext();
                
                if (keymapXml.isEndElement() && keymapXml.name() == QLatin1String("outArr")) {
                    break;
                }
                
                if (!keymapXml.isStartElement()) {
                    continue;
                }
                
                if (keymapXml.name() == QLatin1String("out")) {
                    UserKeyKSh output;
                    while (!keymapXml.atEnd()) {
                        keymapXml.readNext();
                        
                        if (keymapXml.isEndElement() && keymapXml.name() == QLatin1String("out")) {
                            break;
                        }
                        
                        if (!keymapXml.isStartElement()) {
                            continue;
                        }
                        
                        if (keymapXml.name() == QLatin1String("mod")) {
                            QString modifierText = keymapXml.readElementText();
                            if (!modifierText.isEmpty()) {
                                output.modifierTokens = modifierText.split(QLatin1String("|"), Qt::SkipEmptyParts);
                            }
                        }
                        else if (keymapXml.name() == QLatin1String("code")) {
                            output.scancodeToken = keymapXml.readElementText();
                        }
                    }
                    combo.outputs.append(output);
                }
            }
        }
    }
    
    return true;
}
