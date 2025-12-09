#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

#include "streaming/input/userkeycombos.h"

class UserKeyComboBridge : public QObject
{
    Q_OBJECT
public:
    explicit UserKeyComboBridge(QObject* parent = nullptr);

    Q_INVOKABLE QVariantList loadCombos() const;
    Q_INVOKABLE bool reloadCombos() const;
    Q_INVOKABLE bool saveCombos(const QVariantList& combos) const;
    Q_INVOKABLE QVariantMap emptyKeySpec() const;
    Q_INVOKABLE QVariantMap emptyCombo() const;
    Q_INVOKABLE QString keyEventToTokens(int nativeScancode, int nativeModifiers);
    Q_INVOKABLE QString keymapPath() const;

signals:
    void keyCaptured(const QVariantList& keyData);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QString m_capturedKeyCombo;
    bool m_capturingKey = false;
    SDL_Keymod nativeModifiersToSDL(int nativeModifiers, SDL_Scancode scancode);

};
