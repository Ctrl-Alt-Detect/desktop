#include "debug_overlay_renderer.h"
#include <QTextEdit>
#include <QTextCursor>
#include <QScrollBar>

DebugOverlayRenderer::DebugOverlayRenderer(QWidget* parentForWidget)
    : m_overlayLabel(nullptr), m_debugEnabled(false), m_debugScrollPosition(0) {
    initializeOverlayWidget(parentForWidget);
}

void DebugOverlayRenderer::initializeOverlayWidget(QWidget* parent) {
    if (!parent) {
        return;
    }

    m_overlayLabel = new QTextEdit(parent);
    m_overlayLabel->setObjectName("debugOverlay");
    m_overlayLabel->setReadOnly(true);
    m_overlayLabel->setLineWrapMode(QTextEdit::NoWrap);
    m_overlayLabel->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_overlayLabel->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_overlayLabel->hide();

    // Set stylesheet for dark background, light text
    m_overlayLabel->setStyleSheet(
        "QTextEdit {"
        "  background-color: rgba(0, 0, 0, 220);"
        "  color: #00FF00;"
        "  border: 2px solid #00AA00;"
        "  border-radius: 4px;"
        "  font-family: 'Courier New', Courier, monospace;"
        "  font-size: 14px;"
        "  font-weight: bold;"
        "  padding: 8px;"
        "  margin: 0px;"
        "}"
        "QTextEdit:focus {"
        "  border: 2px solid #00FF00;"
        "}"
    );
}

void DebugOverlayRenderer::toggleDebugOverlay() {
    m_debugEnabled = !m_debugEnabled;
    if (!m_debugEnabled) {
        clear();
    }
}

void DebugOverlayRenderer::setEnabled(bool enabled) {
    m_debugEnabled = enabled;
    if (!m_debugEnabled) {
        clear();
    }
}

void DebugOverlayRenderer::clear() {
    if (m_overlayLabel) {
        m_overlayLabel->clear();
        m_overlayLabel->hide();
        m_debugScrollPosition = 0;
    }
    m_hasContent = false;
}

void DebugOverlayRenderer::updateDebugInfo(const QString& debugInfo) {
    if (!m_overlayLabel) {
        return;
    }

    if (!m_debugEnabled || debugInfo.isEmpty()) {
        clear();
        return;
    }

    m_hasContent = true;

    // Save scroll position before updating
    const int scrollPos = m_overlayLabel->verticalScrollBar()->value();

    // Block signals to prevent automatic scrolling during update
    const bool wasBlocked = m_overlayLabel->blockSignals(true);
    m_overlayLabel->verticalScrollBar()->blockSignals(true);

    // Use cursor operations for efficient text replacement
    QTextCursor cursor(m_overlayLabel->document());
    cursor.beginEditBlock();
    cursor.select(QTextCursor::Document);
    cursor.insertText(debugInfo);
    cursor.endEditBlock();

    // Restore signal blocking
    m_overlayLabel->blockSignals(wasBlocked);
    m_overlayLabel->verticalScrollBar()->blockSignals(false);

    // Restore scroll position
    m_overlayLabel->verticalScrollBar()->setValue(scrollPos);

    // Show overlay
    m_overlayLabel->show();
    m_overlayLabel->raise();
}
