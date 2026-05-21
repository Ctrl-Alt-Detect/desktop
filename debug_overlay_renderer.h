#ifndef DEBUG_OVERLAY_RENDERER_H
#define DEBUG_OVERLAY_RENDERER_H

#include <QString>
#include <QWidget>
#include <QTextEdit>

/**
 * DebugOverlayRenderer - Manages on-screen debug information display (F3 toggle)
 * 
 * Encapsulates the logic for:
 * - Debug overlay widget creation and styling
 * - F3 toggle enable/disable
 * - Debug info text rendering with scroll position preservation
 * - Overlay visibility and layering
 */
class DebugOverlayRenderer {
public:
    explicit DebugOverlayRenderer(QWidget* parentForWidget = nullptr);
    ~DebugOverlayRenderer() = default;

    /**
     * Get the debug overlay widget
     */
    QTextEdit* widget() const { return m_overlayLabel; }

    /**
     * Check if debug overlay is enabled
     */
    bool isEnabled() const { return m_debugEnabled; }

    /**
     * Toggle debug overlay on/off
     */
    void toggleDebugOverlay();

    /**
     * Update debug overlay with new information text
     * Preserves scroll position and rendering efficiency
     * 
     * @param debugInfo Text containing debug information (typically multi-line)
     */
    void updateDebugInfo(const QString& debugInfo);

    /**
     * Explicitly enable/disable debug overlay
     */
    void setEnabled(bool enabled);

    /**
     * Clear overlay and hide widget
     */
    void clear();

    /**
     * Check if overlay has content to display
     */
    bool hasContent() const { return m_hasContent; }

private:
    QTextEdit* m_overlayLabel{nullptr};
    bool m_debugEnabled{false};
    bool m_hasContent{false};
    int m_debugScrollPosition{0};

    void initializeOverlayWidget(QWidget* parent);
};

#endif // DEBUG_OVERLAY_RENDERER_H
