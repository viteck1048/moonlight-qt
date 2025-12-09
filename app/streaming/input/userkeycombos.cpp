#include "streaming/input/userkeycombos.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <utility>

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
    if (!ensureKeymapPath()) {
        return false;
    }
    return loadUserKeyCombos();
}

bool UserKeyComboManager::reload()
{
    return loadUserKeyCombos();
}

bool UserKeyComboManager::loadUserKeyCombos()
{
    if (!ensureKeymapPath()) {
        return false;
    }
    
    QFile keymapFile(m_KeymapPath);
    if (!keymapFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Keymap config not loaded (path: %s): %s",
                    m_KeymapPath.toUtf8().constData(),
                    keymapFile.errorString().toUtf8().constData());
        return false;
    }
    
    // Clear the old intermediate array
    m_UserKeyCombosInterm.clear();
    
    QXmlStreamReader keymapXml(&keymapFile);
    while (!keymapXml.atEnd() && !keymapXml.hasError()) {
        keymapXml.readNext();
        
        if (!keymapXml.isStartElement()) {
            continue;
        }
        
        if (keymapXml.name() == QLatin1String("userKeyCombo")) {
            UserKeyComboInterm combo;
            if (parseUserKeyComboElement(keymapXml, combo)) {
                m_UserKeyCombosInterm.append(combo);
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
    if (m_KeymapPath.isEmpty()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Cannot save key combos: keymap path not initialized");
        return false;
    }
    
    QFile file(m_KeymapPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to open keymap file for writing (%s): %s",
                    m_KeymapPath.toUtf8().constData(),
                    file.errorString().toUtf8().constData());
        return false;
    }
    
    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("userKeyCombos"));
    
    for (const UserKeyComboInterm& combo : m_UserKeyCombosInterm) {
        writer.writeStartElement(QStringLiteral("userKeyCombo"));
        
        // Input
        writer.writeStartElement(QStringLiteral("in"));
        writer.writeTextElement(QStringLiteral("mod"), combo.input.modifierTokens.join(QLatin1String("|")));
        writer.writeTextElement(QStringLiteral("code"), combo.input.scancodeToken);
        writer.writeEndElement();
        
        // Description
        if (!combo.description.isEmpty()) {
            writer.writeTextElement(QStringLiteral("description"), combo.description);
        }
        
        // Outputs
        writer.writeStartElement(QStringLiteral("outArr"));
        for (const UserKeyKSh& output : combo.outputs) {
            writer.writeStartElement(QStringLiteral("out"));
            writer.writeTextElement(QStringLiteral("mod"), output.modifierTokens.join(QLatin1String("|")));
            writer.writeTextElement(QStringLiteral("code"), output.scancodeToken);
            writer.writeEndElement();
        }
        writer.writeEndElement();
        
        writer.writeEndElement();
    }
    
    writer.writeEndElement();
    writer.writeEndDocument();
    
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

bool UserKeyComboManager::ensureKeymapPath()
{
    QString keymapDirectory = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (keymapDirectory.isEmpty()) {
        keymapDirectory = QCoreApplication::applicationDirPath();
    }
    
    QDir keymapDir(keymapDirectory);
    if (!keymapDir.exists() && !keymapDir.mkpath(".")) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Failed to create keymap directory (%s). Falling back to application dir.",
                    keymapDirectory.toUtf8().constData());
        keymapDir = QDir(QCoreApplication::applicationDirPath());
    }
    
    const QString keymapPath = keymapDir.filePath(QStringLiteral("keymap.xml"));
    
    if (!QFile::exists(keymapPath)) {
        QFileInfo keymapInfo(keymapPath);
        if (!keymapInfo.dir().exists() && !QDir().mkpath(keymapInfo.dir().absolutePath())) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to create keymap directory for template (%s)",
                        keymapPath.toUtf8().constData());
            return false;
        }
        
        QFile templateFile(keymapPath);
        if (!templateFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Failed to create keymap template: %s",
                        templateFile.errorString().toUtf8().constData());
            return false;
        }
        
        QTextStream out(&templateFile);
        out << "<!-- Moonlight user key combos -->\n";
        out << "<userKeyCombos>\n";
        out << "    <!--\n";
        out << "    <userKeyCombo>\n";
        out << "        <in>\n";
        out << "            <mod>KMOD_LCTRL|KMOD_LALT</mod>\n";
        out << "            <code>SDL_SCANCODE_F1</code>\n";
        out << "        </in>\n";
        out << "        <description>Optional description</description>\n";
        out << "        <outArr>\n";
        out << "            <out>\n";
        out << "                <mod>KMOD_LCTRL|KMOD_LSHIFT</mod>\n";
        out << "                <code>SDL_SCANCODE_DELETE</code>\n";
        out << "            </out>\n";
        out << "        </outArr>\n";
        out << "    </userKeyCombo>\n";
        out << "    -->\n";
        out << "    <!--\n";
        out << "    <userKeyCombo>\n";
        out << "        <in>\n";
        out << "            <mod>KMOD_LCTRL|KMOD_LALT</mod>\n";
        out << "            <code>SDL_SCANCODE_F2</code>\n";
        out << "        </in>\n";
        out << "        <description></description>\n";
        out << "        <outArr>\n";
        out << "            <out>\n";
        out << "                <mod>KMOD_LCTRL|KMOD_LSHIFT</mod>\n";
        out << "                <code>SDL_SCANCODE_DELETE</code>\n";
        out << "            </out>\n";
        out << "            <out>\n";
        out << "                <mod>KMOD_LCTRL|KMOD_LSHIFT</mod>\n";
        out << "                <code>SDL_SCANCODE_L</code>\n";
        out << "            </out>\n";
        out << "        </outArr>\n";
        out << "    </userKeyCombo>\n";
        out << "    -->\n";
        out << "</userKeyCombos>\n";
        
        templateFile.close();
    }
    
    m_KeymapPath = keymapPath;
    return true;
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
