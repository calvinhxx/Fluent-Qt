"""Source helpers for the compact, responsive input examples."""

from textwrap import dedent


def input_columns_script(body: str) -> str:
    return dedent('''\
        from PySide6.QtCore import QSize, Qt
        from PySide6.QtWidgets import QBoxLayout, QLayout, QSizePolicy, QVBoxLayout, QWidget

        class InputSampleLayout(QBoxLayout):
            row_width = 2 * 370 + fluentqt.Spacing.Large

            def __init__(self, parent):
                super().__init__(QBoxLayout.LeftToRight, parent)
                self.setContentsMargins(0, 0, 0, 0)
                self.setSpacing(fluentqt.Spacing.Large)
                self.setSizeConstraint(QLayout.SetNoConstraint)
                self.setAlignment(Qt.AlignTop)

            def hasHeightForWidth(self):
                return True

            def heightForWidth(self, width):
                heights = []
                for index in range(self.count()):
                    column = self.itemAt(index).widget()
                    if column is not None:
                        heights.append(
                            column.heightForWidth(min(width, 370))
                            if column.hasHeightForWidth() else column.sizeHint().height()
                        )
                if width < self.row_width:
                    return sum(heights) + max(0, len(heights) - 1) * fluentqt.Spacing.Large
                return max(heights, default=0)

            def sizeHint(self):
                return QSize(self.row_width, self.heightForWidth(self.row_width))

            def minimumSize(self):
                return QSize(180, self.heightForWidth(self.row_width))

            def setGeometry(self, rect):
                self.setDirection(
                    QBoxLayout.TopToBottom if rect.width() < self.row_width
                    else QBoxLayout.LeftToRight
                )
                super().setGeometry(rect)

        class InputSampleColumns(QWidget):
            def __init__(self, parent=None):
                super().__init__(parent)
                self.setMaximumWidth(InputSampleLayout.row_width)
                self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
                self.box = InputSampleLayout(self)

            def add_column(self):
                column = QWidget(self)
                column.setMaximumWidth(370)
                column.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
                layout = QVBoxLayout(column)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(fluentqt.Spacing.Small)
                self.box.addWidget(column, 1, Qt.AlignTop)
                return column

        ''') + '\n' + dedent(body).strip() + '\n'
