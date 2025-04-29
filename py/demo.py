import sys
from PySide6.QtGui import QColor
from PySide6.QtWidgets import QApplication, QWidget, QVBoxLayout

import EverloadTags as ET


class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("EverloadTags Demo")

        self.config = ET.Config()

        self.config.style.color = QColor(255, 0, 255, 255)
        self.tags = ["tag1", "tag2", "tag3", "tag4", "tag5"]
        self.edit_tags = ["edit1", "edit2", "edit3"]
        self.complete_tags = [
            "complete1",
            "complete2",
            "complete3",
            "complete4",
            "complete5",
            "complete6",
        ]

        self.main_layout = QVBoxLayout()
        self.tag_line_edit = ET.TagsLineEdit(config=self.config)
        self.tag_line_edit.tags(self.tags)
        self.tag_line_edit.completion(self.complete_tags)
        self.main_layout.addWidget(self.tag_line_edit)
        self.tag_edit = ET.TagsEdit()
        self.tag_edit.tags(self.edit_tags)
        self.tag_edit.completion(self.complete_tags)
        self.main_layout.addWidget(self.tag_edit)
        self.setLayout(self.main_layout)
        self.setMinimumSize(320, 240)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec())
