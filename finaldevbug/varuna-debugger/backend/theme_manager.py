# -----------------------------------------------------------------
# File : backend/theme_manager.py
# Phase : Phase 3 Step 3
# -----------------------------------------------------------------

from PySide6.QtCore import QObject, Property, Signal
from PySide6.QtGui import QColor


class ThemeManager(QObject):

    changed = Signal()
    animDurationChanged = Signal()

    def __init__(self, settings_manager, parent=None):
        super().__init__(parent)
        self._settings = settings_manager
        self._settings.darkModeChanged.connect(self._on_theme_changed)
        self._settings.animationsEnabledChanged.connect(self._on_anim_changed)

    def _on_theme_changed(self):
        print(f"Theme changed: dark={self._settings.darkMode}")
        self.changed.emit()

    def _on_anim_changed(self):
        print(f"Animation duration changed: enabled={self._settings.animationsEnabled}")
        self.animDurationChanged.emit()

    def _dark(self):
        return self._settings.darkMode

    @Property(bool, notify=changed)
    def dark(self):
        return self._dark()

    @Property(int, notify=animDurationChanged)
    def animDuration(self):
        return 200 if self._settings.animationsEnabled else 0

    @Property(QColor, notify=changed)
    def background(self):
        return QColor("#0f1318") if self._dark() else QColor("#f8f9fa")

    @Property(QColor, notify=changed)
    def surface(self):
        return QColor("#1a1f28") if self._dark() else QColor("#ffffff")

    @Property(QColor, notify=changed)
    def surfaceVariant(self):
        return QColor("#242a35") if self._dark() else QColor("#f1f3f4")

    @Property(QColor, notify=changed)
    def border(self):
        return QColor("#2e3642") if self._dark() else QColor("#dadce0")

    @Property(QColor, notify=changed)
    def primary(self):
        return QColor("#8ab4f8") if self._dark() else QColor("#1a73e8")

    @Property(QColor, notify=changed)
    def primaryContainer(self):
        return QColor("#1a3a5c") if self._dark() else QColor("#d2e3fc")

    @Property(QColor, notify=changed)
    def secondary(self):
        return QColor("#81c995") if self._dark() else QColor("#1e8e3e")

    @Property(QColor, notify=changed)
    def error(self):
        return QColor("#f28b82") if self._dark() else QColor("#d93025")

    @Property(QColor, notify=changed)
    def warning(self):
        return QColor("#fdd663") if self._dark() else QColor("#f9ab00")

    @Property(QColor, notify=changed)
    def tertiary(self):
        return QColor("#f29b74") if self._dark() else QColor("#e8710a")

    @Property(QColor, notify=changed)
    def info(self):
        return QColor("#8ab4f8") if self._dark() else QColor("#1a73e8")

    @Property(QColor, notify=changed)
    def onBackground(self):
        return QColor("#e8eaed") if self._dark() else QColor("#202124")

    @Property(QColor, notify=changed)
    def onSurface(self):
        return QColor("#9aa0a6") if self._dark() else QColor("#5f6368")

    @Property(QColor, notify=changed)
    def onSurfaceVariant(self):
        return QColor("#5f6368") if self._dark() else QColor("#9aa0a6")

    @Property(float, notify=changed)
    def shadowOpacity1(self):
        return 0.12 if self._dark() else 0.08

    @Property(float, notify=changed)
    def shadowOpacity4(self):
        return 0.24 if self._dark() else 0.16

    @Property(float, notify=changed)
    def scrimOpacity(self):
        return 0.5 if self._dark() else 0.32

# -----------------------------------------------------------------
# File : backend/theme_manager.py
# Phase : Phase 3 Step 3
# ----------------------------END----------------------------------
